#include "GASSInstrInfo.h"
#include "GASSRegisterInfo.h"
#include "GASSSubtarget.h"
#include "MCTargetDesc/GASSMCTargetDesc.h"

using namespace llvm;

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
  } else {
    llvm_unreachable("Bad phys reg copy");
  }

  BuildMI(MBB, I, DL, get(Op), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc));
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

unsigned GASSInstrInfo::insertBranch(MachineBasicBlock &MBB,
                                     MachineBasicBlock *TBB,
                                     MachineBasicBlock *FBB,
                                     ArrayRef<MachineOperand> Cond,
                                     const DebugLoc &DL,
                                     int *BytesAdded) const {
  assert(!BytesAdded && "code size not handled");

  // Shouldn't be a fall through.
  assert(TBB && "insertBranch must not be told to insert a fallthrough");
  assert((Cond.size() == 1 || Cond.size() == 0) &&
         "GASS branch conditions have two components!");

  // One-way branch.
  if (!FBB) {
    if (Cond.empty()) // Unconditional branch
      BuildMI(&MBB, DL, get(GASS::BRA)).addMBB(TBB);
    else // Conditional branch
      BuildMI(&MBB, DL, get(GASS::CBRA)).addReg(Cond[0].getReg())
          .addMBB(TBB);
    return 1;
  }

  // Two-way Conditional Branch.
  BuildMI(&MBB, DL, get(GASS::CBRA)).addReg(Cond[0].getReg()).addMBB(TBB);
  BuildMI(&MBB, DL, get(GASS::BRA)).addMBB(FBB);
  return 2;
}

// Expand pseudo instrucitons
// IMUL, IADD64, SEXT
bool GASSInstrInfo::expandPostRAPseudo(MachineInstr &MI) const {
  unsigned Opc = MI.getOpcode();

  MachineBasicBlock &MBB = *MI.getParent();
  auto &Subtarget = MBB.getParent()->getSubtarget<GASSSubtarget>();
  auto TRI = Subtarget.getRegisterInfo();
  DebugLoc DL = MI.getDebugLoc();

  switch (Opc){
  default: return false;
  // case GASS::IADD64rr: {
  //   // IADD64rr -> IADD32 + IADD32.x
  //   Register Dst = MI.getOperand(0).getReg();
  //   Register DstLo = TRI->getSubReg(Dst, GASS::sub0);
  //   Register DstHi = TRI->getSubReg(Dst, GASS::sub1);

  //   Register LHS = MI.getOperand(1).getReg();
  //   Register LHSLo = TRI->getSubReg(LHS, GASS::sub0);
  //   Register LHSHi = TRI->getSubReg(LHS, GASS::sub1);

  //   Register RHS = MI.getOperand(2).getReg();
  //   Register RHSLo = TRI->getSubReg(RHS, GASS::sub0);
  //   Register RHSHi = TRI->getSubReg(RHS, GASS::sub1);

  //   // FIXME: We preserve P6 as carry register 
  //   // Chain these two instructions
  //   BuildMI(MBB, MI, DL, get(GASS::IADDXrr))
  //     .addReg(DstLo, RegState::Define)
  //     .addReg(GASS::PR6)
  //     .addReg(LHSLo)
  //     .addReg(RHSLo)
  //     .addReg(GASS::NPT); // !PT
  //   BuildMI(MBB, MI, DL, get(GASS::IADDXrr))
  //     .addReg(DstHi, RegState::Define)
  //     .addReg(GASS::PT)
  //     .addReg(LHSHi)
  //     .addReg(RHSHi)
  //     .addReg(GASS::PR6);
  // } break;
  case GASS::IMULrr: { /* TODO */ }
    break;
  case GASS::IMUL_WIDErr: {/* TODO */}
    break;
  case GASS::IMUL_WIDEri: {
    Register Dst = MI.getOperand(0).getReg();
    BuildMI(MBB, MI, DL, get(GASS::IMAD_S32_WIDErir), Dst)
      .add(MI.getOperand(1))
      .add(MI.getOperand(2))
      .addReg(GASS::RZ64);
  } break;
  }

  MI.eraseFromParent();

  return true;
}

//=----------------------------------------------=//
// Query instr type
//=----------------------------------------------=//
bool GASSInstrInfo::isLoad(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  default:
    break;
  case GASS::LDC32c: case GASS::LDC64c:
  case GASS::LDG32r: case GASS::LDG32ri:
  case GASS::LDG64r: case GASS::LDG64ri:
    return true;
  }
  return false;
}

bool GASSInstrInfo::isStore(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  default:
    break;
  case GASS::STG32r: case GASS::STG32ri:
  case GASS::STG64r: case GASS::STG64ri:
    return true;
  }
  return false;
}

MachineOperand* GASSInstrInfo::getMemOperandReg(MachineInstr &MI) {
  MachineOperand *Ret = nullptr;
  switch (MI.getOpcode()) {
  default: return Ret;
  case GASS::LDG32r: case GASS::LDG32ri:
    return &MI.getOperand(1);
  case GASS::STG32r: case GASS::STG32ri:
  case GASS::STG64r: case GASS::STG64ri:
    return &MI.getOperand(0);
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

void GASSInstrInfo::encodeBarrierMask(MachineInstr &MI, unsigned BarMask) {
  assert(BarMask < 64 && "Wait barrier mask should be smaller than 64");
  uint16_t Flags = MI.getFlags();
  Flags &= ~(0b11'1111 << 10);
  Flags |= BarMask << 10;
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