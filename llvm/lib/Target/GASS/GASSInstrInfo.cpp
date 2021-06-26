#include "GASS.h"
#include "GASSInstrInfo.h"
#include "GASSRegisterInfo.h"
#include "GASSSubtarget.h"
#include "MCTargetDesc/GASSMCTargetDesc.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/Support/ErrorHandling.h"
#include <functional>

using namespace llvm;

#define DEBUG_TYPE "gass-instr-info"

#define GET_INSTRINFO_CTOR_DTOR
#include "GASSGenInstrInfo.inc"

// See NVPTXInstrInfo::copyPhysReg()
void GASSInstrInfo::copyPhysReg(MachineBasicBlock &MBB, 
                                MachineBasicBlock::iterator I,
                                const DebugLoc &DL, MCRegister DestReg, 
                                MCRegister SrcReg, bool KillSrc) const {
  unsigned Op;

  if (GASS::VReg32RegClass.contains(SrcReg) &&
      GASS::VReg32RegClass.contains(DestReg)) {
    Op = GASS::MOV32r;
  } else if (GASS::VReg64RegClass.contains(SrcReg) &&
             GASS::VReg64RegClass.contains(DestReg)) {
    Op = GASS::MOV64r;
  } 
  // VReg32 = VReg16
  else if (GASS::VReg16RegClass.contains(SrcReg) &&
           GASS::VReg32RegClass.contains(DestReg)) {
    assert(GASS::VReg16LoRegClass.contains(SrcReg) || 
           GASS::VReg16HiRegClass.contains(SrcReg));
    if (GASS::VReg16LoRegClass.contains(SrcReg)) {
      // dst = prmt 
    } else {
      // dst = ???
    }
    llvm_unreachable("Not implemented");
  } else
    llvm_unreachable("Bad phys reg copy");

  BuildMI(MBB, I, DL, get(Op), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc))
      .addImm(0).addReg(GASS::PT);
}

//=----------------------------------------------------------------------=//
// branch analysis
//=----------------------------------------------------------------------=//
/// analyzeBranch - Analyze the branching code at the end of MBB, returning
/// true if it cannot be understood (e.g. it's a switch dispatch or isn't
/// implemented for a target).  Upon success, this returns false and returns
/// with the following information in various cases:
///
/// 1. If this block ends with no branches (it just falls through to its succ)
///    just return false, leaving TBB/FBB null.
/// 2. If this block ends with only an unconditional branch, it sets TBB to be
///    the destination block.
/// 3. If this block ends with an conditional branch and it falls through to
///    an successor block, it sets TBB to be the branch destination block and a
///    list of operands that evaluate the condition. These
///    operands can be passed to other TargetInstrInfo methods to create new
///    branches.
/// 4. If this block ends with an conditional branch and an unconditional
///    block, it returns the 'true' destination in TBB, the 'false' destination
///    in FBB, and a list of operands that evaluate the condition. These
///    operands can be passed to other TargetInstrInfo methods to create new
///    branches.
///
/// Note that removeBranch and insertBranch must be implemented to support
/// cases where this method returns success.
///
bool GASSInstrInfo::analyzeBranch(MachineBasicBlock &MBB,
                                  MachineBasicBlock *&TBB,
                                  MachineBasicBlock *&FBB,
                                  SmallVectorImpl<MachineOperand> &Cond,
                                  bool AllowModify) const {
  // If the block has no terminators, it just falls into the block after it.
  MachineBasicBlock::iterator I = MBB.end();
  if (I == MBB.begin() || !isUnpredicatedTerminator(*--I))
    return false;

  // Get the last instruction in the block.
  MachineInstr &LastInst = *I;

  // If there is only one terminator instruction, process it.
  if (I == MBB.begin() || !isUnpredicatedTerminator(*--I)) {
    if (LastInst.getOpcode() == GASS::BRA) {
      TBB = LastInst.getOperand(0).getMBB();
      return false;
    } else if (LastInst.getOpcode() == GASS::CBRA) {
      // Block ends with fall-through condbranch.
      TBB = LastInst.getOperand(1).getMBB();
      Cond.push_back(MachineOperand::CreateImm(0)); // If cond flipped
      Cond.push_back(LastInst.getOperand(0));
      return false;
    }
    // Otherwise, don't know what this is.
    return true;
  }

  // Get the instruction before it if it's a terminator.
  MachineInstr &SecondLastInst = *I;

  // If there are three terminators, we don't know what sort of block this is.
  if (I != MBB.begin() && isUnpredicatedTerminator(*--I))
    return true;

  // If the block ends with CBRA and BRA, handle it.
  if (SecondLastInst.getOpcode() == GASS::CBRA &&
      LastInst.getOpcode() == GASS::BRA) {
    TBB = SecondLastInst.getOperand(1).getMBB();
    Cond.push_back(MachineOperand::CreateImm(0)); // If cond flipped
    Cond.push_back(SecondLastInst.getOperand(0));
    FBB = LastInst.getOperand(0).getMBB();
    return false;
  }

  // If the block ends with two GASS::BRAs, handle it.  The second one is not
  // executed, so remove it.
  if (SecondLastInst.getOpcode() == GASS::BRA &&
      LastInst.getOpcode() == GASS::BRA) {
    TBB = SecondLastInst.getOperand(0).getMBB();
    I = LastInst;
    if (AllowModify)
      I->eraseFromParent();
    return false;
  }

  // Otherwise, can't handle this.
  return true;
}

// Note: return false on success
// Now: can not reverse branch condition
bool
GASSInstrInfo::reverseBranchCondition(
    SmallVectorImpl<MachineOperand> &Cond) const {
  assert(Cond.size() == 2 && "Invalid branch condition!");
  Cond[0].setImm(Cond[0].getImm() ^ 1ul);
  return false;
}

// returns true on success
bool GASSInstrInfo::PredicateInstruction(MachineInstr &MI, 
                                         ArrayRef<MachineOperand> Pred) const {
  assert(MI.isPredicable() && "Expect predicable instruction");
  assert(Pred.size() == 2);
  assert(Pred[0].isImm() && Pred[1].isReg());

  int PIdx = MI.findFirstPredOperandIdx();

  if (PIdx != -1) {
    MI.getOperand(PIdx).setImm(Pred[0].getImm());
    MI.getOperand(PIdx+1).setReg(Pred[1].getReg());
    return true;
  }

  return false;
}

/// Remove the branching code at the end of the specific MBB.
/// This is only invoked in cases where analyzeBranch returns success. It
/// returns the number of instructions that were removed.
/// If \p BytesRemoved is non-null, report the change in code size from the
/// removed instructions.
unsigned GASSInstrInfo::removeBranch(MachineBasicBlock &MBB,
                                     int *BytesRemoved) const {
  assert(!BytesRemoved && "code size not handled");
  MachineBasicBlock::iterator I = MBB.end();
  if (I == MBB.begin())
    return 0;
  --I;
  if (I->getOpcode() != GASS::BRA && I->getOpcode() != GASS::CBRA)
    return 0;

  // Remove the branch.
  I->eraseFromParent();

  I = MBB.end();

  if (I == MBB.begin())
    return 1;
  --I;
  if (I->getOpcode() != GASS::CBRA)
    return 1;

  // Remove the branch.
  I->eraseFromParent();
  return 2;
}

/// Return true if it is possible to insert a select
/// instruction that chooses between TrueReg and FalseReg based on the
/// condition code in Cond.
///
/// When successful, also return the latency in cycles from TrueReg,
/// FalseReg, and Cond to the destination register. In most cases, a select
/// instruction will be 1 cycle, so CondCycles = TrueCycles = FalseCycles = 1
///
/// @param MBB         Block where select instruction would be inserted.
/// @param Cond        Condition returned by analyzeBranch.
/// @param DstReg      Virtual dest register that the result should write to.
/// @param TrueReg     Virtual register to select when Cond is true.
/// @param FalseReg    Virtual register to select when Cond is false.
/// @param CondCycles  Latency from Cond+Branch to select output.
/// @param TrueCycles  Latency from TrueReg to select output.
/// @param FalseCycles Latency from FalseReg to select output.
bool GASSInstrInfo::canInsertSelect(const MachineBasicBlock &MBB,
                                    ArrayRef<MachineOperand> Cond,
                                    Register DstReg, Register TrueReg,
                                    Register FalseReg, int &CondCycles,
                                    int &TrueCycles, int &FalseCycles) const {
  // Check register classes.
  const MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
  const TargetRegisterClass *RC = 
    RI.getCommonSubClass(MRI.getRegClass(TrueReg), MRI.getRegClass(FalseReg));
  if (!RC)
    return false;

  // XXX: What's this? (Ref::X86InstrInfo::canInsertSelect)
  if (GASS::VReg32RegClass.hasSubClassEq(RC) || 
      GASS::VReg64RegClass.hasSubClassEq(RC) ||
      GASS::VReg128RegClass.hasSubClassEq(RC)) {
    // FIXME: Update latencies
    CondCycles = 10;
    TrueCycles = 5;
    FalseCycles = 5;
    return true;
  }

  // Can't do vector
  return false;
}

/// Insert a select instruction into MBB before I that will copy TrueReg to
/// DstReg when Cond is true, and FalseReg to DstReg when Cond is false.
///
/// This function can only be called after canInsertSelect() returned true.
/// The condition in Cond comes from analyzeBranch, and it can be assumed
/// that the same flags or registers required by Cond are available at the
/// insertion point.
///
/// @param MBB      Block where select instruction should be inserted.
/// @param I        Insertion point.
/// @param DL       Source location for debugging.
/// @param DstReg   Virtual register to be defined by select instruction.
/// @param Cond     Condition as computed by analyzeBranch.
/// @param TrueReg  Virtual register to copy when Cond is true.
/// @param FalseReg Virtual register to copy when Cons is false.
void GASSInstrInfo::insertSelect(MachineBasicBlock &MBB,
                          MachineBasicBlock::iterator I, const DebugLoc &DL,
                          Register DstReg, ArrayRef<MachineOperand> Cond,
                          Register TrueReg, Register FalseReg) const {
  llvm_unreachable("Not implemented");
  
}

unsigned GASSInstrInfo::insertBranch(MachineBasicBlock &MBB,
                                     MachineBasicBlock *TBB,
                                     MachineBasicBlock *FBB,
                                     ArrayRef<MachineOperand> Cond,
                                     const DebugLoc &DL,
                                     int *BytesAdded) const {
  assert(!BytesAdded && "code size not handled");

  // Shouldn't be a fall through.
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert((Cond.size() == 2 || Cond.size() == 0) &&
         "GASS branch conditions invalid!");

  // One-way branch.
  if (!FBB) {
    if (Cond.empty()) // Unconditional branch
      BuildMI(&MBB, DL, get(GASS::BRA)).addMBB(TBB);
    else // Conditional branch
      BuildMI(&MBB, DL, get(GASS::CBRA)).addReg(Cond[1].getReg())
          .addMBB(TBB);
    return 1;
  }

  // Two-way Conditional Branch.
  BuildMI(&MBB, DL, get(GASS::CBRA)).addReg(Cond[1].getReg()).addMBB(TBB);
  BuildMI(&MBB, DL, get(GASS::BRA)).addMBB(FBB);
  return 2;
}

/// Helper function for LOP3, immLut generator
static char makeLop3Lut(std::function<char(char, char)> fab, 
                        std::function<char(char, char)> fbc) {
  uint8_t TA = 0xF0;
  uint8_t TB = 0xCC;
  uint8_t TC = 0xAA;
  
  return fbc(fab(TA, TB), TC);
}

// Expand pseudo instrucitons
// IMUL, IADD64, SEXT
bool GASSInstrInfo::expandPostRAPseudo(MachineInstr &MI) const {
  unsigned Opc = MI.getOpcode();

  MachineBasicBlock &MBB = *MI.getParent();
  auto &Subtarget = MBB.getParent()->getSubtarget<GASSSubtarget>();
  const GASSRegisterInfo *TRI = Subtarget.getRegisterInfo();
  DebugLoc DL = MI.getDebugLoc();

  switch (Opc){
  default: return false;
  case GASS::IMULrr: { 
    Register Dst = MI.getOperand(0).getReg();
    BuildMI(MBB, MI, DL, get(GASS::IMAD_S32rrr), Dst)
      .add(MI.getOperand(1))
      .add(MI.getOperand(2))
      .addReg(GASS::RZ32)
      .add(MI.getOperand(3)).add(MI.getOperand(4)); // PredMask
  } break;
  case GASS::IMUL_WIDErr: {
    llvm_unreachable("Not implemented");
  } break;
  case GASS::IMUL_WIDEri: {
    Register Dst = MI.getOperand(0).getReg();
    BuildMI(MBB, MI, DL, get(GASS::IMAD_S32_WIDErir), Dst)
      .add(MI.getOperand(1))
      .add(MI.getOperand(2))
      .addReg(GASS::RZ64)
      .add(MI.getOperand(3)).add(MI.getOperand(4)); // PredMask
  } break;
  case GASS::OR1rr: case GASS::OR1ri: 
  case GASS::XOR1rr: case GASS::XOR1ri: 
  case GASS::AND1rr: case GASS::AND1ri: 
  {
    llvm_unreachable("Not implemented");
    // OR,XOR,AND -> LOP3
  } break;
  case GASS::OR32rr: case GASS::OR32ri:
  case GASS::XOR32rr: case GASS::XOR32ri:
  case GASS::AND32rr: case GASS::AND32ri: {
    // Note: LOP3.LUT $dst, $src0, $src1, $src2, $immLut;
    Register Dst = MI.getOperand(0).getReg();
    unsigned Opcode;
    switch (Opc) {
    llvm_unreachable("Shouldn't be here");
    case GASS::OR32rr: case GASS::XOR32rr: case GASS::AND32rr:
      Opcode = GASS::LOP3rrr; 
      break;
    case GASS::OR32ri: case GASS::XOR32ri: case GASS::AND32ri:
      Opcode = GASS::LOP3rir;
      break;
    }
    
    char ImmLut = 0;
    // OR -> LOP3 
    // a or b; --> a or b or rz;
    if (Opc == GASS::OR32rr || Opc == GASS::OR32ri) {
      ImmLut = makeLop3Lut(std::bit_or<char>(), std::bit_or<char>());
    } else if (Opc == GASS::XOR32rr || Opc == GASS::XOR32ri) {
      // XOR -> LOP3
      // a xor b; --> a xor b or rz;
      ImmLut = makeLop3Lut(std::bit_xor<char>(), std::bit_or<char>());
    } else if (Opc == GASS::AND32rr || Opc == GASS::AND32ri) {
      // AND -> LOP3
      // a and b; --> a and b or rz;
      ImmLut = makeLop3Lut(std::bit_and<char>(), std::bit_or<char>());
    } else
      llvm_unreachable("Error");

    BuildMI(MBB, MI, DL, get(Opcode), Dst)
      .add(MI.getOperand(1))
      .add(MI.getOperand(2))
      .addReg(GASS::RZ32)
      .addImm(ImmLut)
      .add(MI.getOperand(3)).add(MI.getOperand(4)); // PredMask
  } break;
  case GASS::SHL64rr: case GASS::SHL64ri: {
    // SHL64 $dst, $src, $amt;
    //  -->
    // SHF.L.U64.HI $dst.hi, $src.lo, $amt, $src.hi;
    // SHF.L.U32    $dst.lo, $src.lo, $amt, RZ;
    Register Dst = MI.getOperand(0).getReg();
    Register DstLo = TRI->getSubReg(Dst, GASS::sub0);
    Register DstHi = TRI->getSubReg(Dst, GASS::sub1);

    Register Src = MI.getOperand(1).getReg();
    Register SrcLo = TRI->getSubReg(Src, GASS::sub0);
    Register SrcHi = TRI->getSubReg(Src, GASS::sub1);

    const MachineOperand &Amount = MI.getOperand(2);
    unsigned Opcode;
    if (Amount.isReg()) 
      Opcode = GASS::SHFrrr;
    else if (Amount.isImm())
      Opcode = GASS::SHFrir;
    else
      llvm_unreachable("Invalid data type");

    // SHF.L.U64.HI $dst.hi, $src.lo, $amt, $src.hi;
    BuildMI(MBB, MI, DL, get(Opcode), DstHi)
      .addReg(SrcLo)
      .add(Amount)
      .addReg(SrcHi)
      .addImm(GASS::SHF_FLAGS::L)
      .addImm(GASS::SHF_FLAGS::U64)
      .addImm(GASS::SHF_FLAGS::HI)
      .add(MI.getOperand(3)).add(MI.getOperand(4)); // PredMask

    // SHF.L.U32
    BuildMI(MBB, MI, DL, get(Opcode), DstLo)
      .addReg(SrcLo)
      .add(Amount)
      .addReg(GASS::RZ32)
      .addImm(GASS::SHF_FLAGS::L)
      .addImm(GASS::SHF_FLAGS::U32)
      .addImm(GASS::SHF_FLAGS::LO)
      .add(MI.getOperand(3)).add(MI.getOperand(4)); // PredMask
  } break;
  case GASS::SHL32rr: case GASS::SHL32ri: {
    // SHL32 $dst, $src, $amt;
    //   ->
    // SHF.L.U32.LO $dst, $src, $amt, RZ;
    Register Dst = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    const MachineOperand &Amount = MI.getOperand(2);

    unsigned Opcode;
    if (Amount.isReg()) 
      Opcode = GASS::SHFrrr;
    else if (Amount.isImm())
      Opcode = GASS::SHFrir;
    else
      llvm_unreachable("Invalid data type");

    BuildMI(MBB, MI, DL, get(Opcode), Dst)
      .addReg(Src)
      .add(Amount)
      .addReg(GASS::RZ32)
      .addImm(GASS::SHF_FLAGS::L)
      .addImm(GASS::SHF_FLAGS::U32)
      .addImm(GASS::SHF_FLAGS::LO)
      .add(MI.getOperand(3)).add(MI.getOperand(4)); // PredMask
  } break;
  case GASS::SRL32rr: case GASS::SRL32ri: {
    // SRL32 $dst, $src, $amt;
    //   ->
    // SHF.R.U32.LO $dst, $src, $amt, RZ;
    Register Dst = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    const MachineOperand &Amount = MI.getOperand(2);

    unsigned Opcode;
    if (Amount.isReg()) 
      Opcode = GASS::SHFrrr;
    else if (Amount.isImm())
      Opcode = GASS::SHFrir;
    else
      llvm_unreachable("Invalid data type");

    BuildMI(MBB, MI, DL, get(Opcode), Dst)
      .addReg(Src)
      .add(Amount)
      .addReg(GASS::RZ32)
      .addImm(GASS::SHF_FLAGS::R)
      .addImm(GASS::SHF_FLAGS::U32)
      .addImm(GASS::SHF_FLAGS::LO)
      .add(MI.getOperand(3)).add(MI.getOperand(4)); // PredMask
  } break;
  case GASS::SRA32rr: case GASS::SRA32ri: {
    // SRA32 $dst, $src, $amt;
    //   ->
    // SHF.R.S32.HI $dst, $RZ, $amt, $src;
    Register Dst = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    const MachineOperand &Amount = MI.getOperand(2);

    unsigned Opcode;
    if (Amount.isReg()) 
      Opcode = GASS::SHFrrr;
    else if (Amount.isImm())
      Opcode = GASS::SHFrir;
    else
      llvm_unreachable("Invalid data type");

    BuildMI(MBB, MI, DL, get(Opcode), Dst)
      .addReg(Src)
      .add(Amount)
      .addReg(GASS::RZ32)
      .addImm(GASS::SHF_FLAGS::R)
      .addImm(GASS::SHF_FLAGS::S32)
      .addImm(GASS::SHF_FLAGS::HI)
      .add(MI.getOperand(3)).add(MI.getOperand(4)); // PredMask
  } break;
  case GASS::SRA64rr: case GASS::SRA64ri:
  case GASS::SRL64rr: case GASS::SRL64ri: {
    llvm_unreachable("Not implemented");
  } break;
  case GASS::EXTRACT_ELEMENT_V2F16: {
    // f16:$dst = extractelt v2f16:$src, 0
    //   =>
    // f16:$dst = prmt 
    Register Dst = MI.getOperand(0).getReg();
    Register Src = MI.getOperand(1).getReg();
    const MachineOperand &IdxOp = MI.getOperand(2);
    assert(IdxOp.isImm());
    unsigned Idx = IdxOp.getImm();

    unsigned PrmtMode = 0;
    assert(Idx == 0 || Idx == 1);
    if (Idx == 0)
      PrmtMode = 0x5410;
    else 
      PrmtMode = 0x5432;
    unsigned Opcode = GASS::PRMTrir;
    BuildMI(MBB, MI, DL, get(Opcode), Dst)
      .addReg(Src)
      .addImm(PrmtMode)
      .addReg(GASS::RZ32)
      .add(MI.getOperand(3)).add(MI.getOperand(4)); // PredMask
  } break;
  }

  MI.eraseFromParent();

  return true;
}

//=----------------------------------------------=//
// Query instr type
//=----------------------------------------------=//
bool GASSInstrInfo::needsRAWBarrier(const MachineInstr &MI) {
  return isLoad(MI) | isTC(MI) | isSFU(MI);
}

bool GASSInstrInfo::isLoad(const MachineInstr &MI) {
  return isLDG(MI) | isLDS(MI) | isLDC(MI);
}

bool GASSInstrInfo::isStore(const MachineInstr &MI) {
  return isSTG(MI) | isSTS(MI);
}

bool GASSInstrInfo::isLDG(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  default: return false;
  case GASS::LDG32r: case GASS::LDG32ri:
  case GASS::LDG64r: case GASS::LDG64ri:
  case GASS::LDG128r: case GASS::LDG128ri:
    return true;
  }
}

bool GASSInstrInfo::isLDS(const MachineInstr &MI) {
  // TODO: fill this.
  switch (MI.getOpcode()) {
  default: return false;
  case GASS::READ_TID_X: case GASS::READ_TID_Y: case GASS::READ_TID_Z:
  case GASS::READ_CTAID_X: case GASS::READ_CTAID_Y: case GASS::READ_CTAID_Z:
  case GASS::READ_LANEID:
  case GASS::LDS16r: case GASS::LDS16ri:
  case GASS::LDS32r: case GASS::LDS32ri:
  case GASS::LDS64r: case GASS::LDS64ri:
  case GASS::LDS128r: case GASS::LDS128ri:
  case GASS::LDSM_x4_ri:
    return true;
  }
}

bool GASSInstrInfo::isLDC(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  default: return false;
  case GASS::LDC32c: case GASS::LDC64c:
    return true;
  }
}

bool GASSInstrInfo::isSTG(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  default:
    break;
  case GASS::STG32r: case GASS::STG32ri:
  case GASS::STG64r: case GASS::STG64ri:
  case GASS::STG128r: case GASS::STG128ri:
    return true;
  }
  return false;
}

bool GASSInstrInfo::isSTS(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  default: return false;
  case GASS::STS16r: case GASS::STS16ri:
  case GASS::STS32r: case GASS::STS32ri:
  case GASS::STS64r: case GASS::STS64ri:
  case GASS::STS128r: case GASS::STS128ri:
    return true;
  }
}

bool GASSInstrInfo::isTC(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  default: return false;
  case GASS::HMMA884_f32_f32:
    return true;
  }
}

bool GASSInstrInfo::isSFU(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  default: return false;
  case GASS::F2F_F16_F32:
    return true;
  }
}

bool GASSInstrInfo::ifReadsCarryIn(const MachineInstr &MI) {
  if (MI.getOpcode() == GASS::IADDXrr || MI.getOpcode() == GASS::IADDXri) {
    // operand 5 is carryin
    const MachineOperand &MO = MI.getOperand(5);
    assert(MO.isReg());

    const MachineBasicBlock &MBB = *MI.getParent();
    auto &Subtarget = MBB.getParent()->getSubtarget<GASSSubtarget>();
    const GASSRegisterInfo *TRI = Subtarget.getRegisterInfo();

    const Register &CarryIn = MO.getReg();
    if (CarryIn.isPhysical()) {
      if (!TRI->isConstantPhysReg(CarryIn))
        return true;
      return false;
    }

    const MachineRegisterInfo &MRI = MBB.getParent()->getRegInfo();
    assert(MRI.getRegClass(CarryIn) == &GASS::VReg1RegClass);
    if (!TRI->isConstantPhysReg(CarryIn))
      return true;
  }
  return false;
}

MachineOperand* GASSInstrInfo::getMemOperandReg(MachineInstr &MI) {
  MachineOperand *Ret = nullptr;
  switch (MI.getOpcode()) {
  default: return Ret;
  case GASS::LDS16r: case GASS::LDS16ri:
  case GASS::LDS32r: case GASS::LDS32ri:
  case GASS::LDS64r: case GASS::LDS64ri:
  case GASS::LDS128r: case GASS::LDS128ri:
  case GASS::LDG32r: case GASS::LDG32ri:
  case GASS::LDG64r: case GASS::LDG64ri:
  case GASS::LDG128r: case GASS::LDG128ri:
  case GASS::LDSM_x4_ri:
    return &MI.getOperand(1);
  case GASS::STS16r: case GASS::STS16ri:
  case GASS::STS32r: case GASS::STS32ri:
  case GASS::STS64r: case GASS::STS64ri:
  case GASS::STS128r: case GASS::STS128ri:
  case GASS::STG32r: case GASS::STG32ri:
  case GASS::STG64r: case GASS::STG64ri:
  case GASS::STG128r: case GASS::STG128ri:
    return &MI.getOperand(1); // First operand is src
  }
}

//=------------------------------------------------=//
// Encoding info (sm_70 ~ )
//=------------------------------------------------=//
// uint16_t flags; // 3+3+6+4 
// Wait Mask (6 bits) :: Read Barrier Idx (3 bits) :: 
// Write Barrier Idx (3 bits) :: Stall Cycles (4 bits)
void GASSInstrInfo::encodeReadBarrier(MachineInstr &MI, unsigned BarIdx) {
  assert(BarIdx < 6 && "Read Barrier should be smaller than 6");
  uint16_t Flags = MI.getFlags();
  Flags &= ~(0b111 << 7); // clear default value
  Flags |= BarIdx << 7;
  MI.setFlags(Flags);
}

void GASSInstrInfo::encodeWriteBarrier(MachineInstr &MI, unsigned BarIdx) {
  assert(BarIdx < 6 && "Write barrier should be smaller than 6");
  uint16_t Flags = MI.getFlags();
  Flags &= ~(0b111 << 4); // clear default value
  Flags |= BarIdx << 4;
  MI.setFlags(Flags);
}

void GASSInstrInfo::encodeBarrierMask(MachineInstr &MI, unsigned BarIdx) {
  assert(BarIdx < 6 && "Wait barrier idx should be smaller than 6");
  uint16_t Flags = MI.getFlags();
  Flags |= 1 << (10 + BarIdx);
  MI.setFlags(Flags);
}

void GASSInstrInfo::encodeStallCycles(MachineInstr &MI, unsigned Stalls) {
  assert(Stalls < 16 && "Stall cycles should be smaller than 16");
  uint16_t Flags = MI.getFlags();
  Flags &= ~(0b1111);
  Flags |= Stalls;
  MI.setFlags(Flags);
}

void GASSInstrInfo::initializeFlagsEncoding(MachineInstr &MI) {
  // Default value:
  // Read & Write Barrier idx = 0b111;
  uint16_t Flags = MI.getFlags();
  Flags |= 0b0000'0011'1111'0000;
  MI.setFlags(Flags);
}

// Decode wait barriers
// -1 means this MI doesn't set any read barrier (WAR)
int GASSInstrInfo::decodeReadBarrier(const MachineInstr &MI) {
  uint16_t Flags = MI.getFlags();
  int RBIdx = (Flags & 0b0000'0011'1000'0000) >> 7;
  assert(RBIdx != 6);
  if (RBIdx == 7)
    return -1;
  return RBIdx;
}

// -1 means this MI doesn't set any write barrier (RAW)
int GASSInstrInfo::decodeWriteBarrier(const MachineInstr &MI) {
  uint16_t Flags = MI.getFlags();
  int RBIdx = (Flags & 0b0000'0000'0111'0000) >> 4;
  assert(RBIdx != 6);
  if (RBIdx == 7)
    return -1;
  return RBIdx;
}

DenseSet<int> GASSInstrInfo::decodeBarrierMask(const MachineInstr &MI) {
  uint16_t Flags = MI.getFlags();
  uint16_t Mask = (Flags & 0b1111'1100'0000'0000) >> 10;
  DenseSet<int> Barriers;
  for (int i=0; i<6; ++i)
    if (Mask & (1<<i))
      Barriers.insert(i);
  
  return Barriers;
}