#include "GASS.h"
#include "GASSInstrInfo.h"
#include "GASSRegisterInfo.h"
#include "GASSSubtarget.h"
#include "GASSTargetMachine.h"
#include "MCTargetDesc/GASSMCTargetDesc.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachinePostDominators.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "gass-machine-instr-combine"
#define CPASS_NAME "GASS MachineInstr Combine"

// I don't know how to rely on the LLVM-IR-level Divergence Analysis.
// That generates sreg->vreg copy, which can not be right
//----------------------------------------------------------------=//
// MachineDivergenceAnalysis
// TODO: Support for 
class MachineDivergenceAnalysis {
  DenseSet<Register> DivergentRegs;
  const GASSInstrInfo *TII = nullptr;
  const MachineRegisterInfo *MRI = nullptr;

  // Internal worklist for divergence propagation
  std::vector<Register> Worklist;
public:
  MachineDivergenceAnalysis(MachineFunction &MF, const GASSInstrInfo *TII);

  bool isDivergent(Register Reg) { return DivergentRegs.contains(Reg); }
  bool isUniform(Register Reg) { return !isDivergent(Reg); }
private:
  void compute();
  void pushUsers(Register Reg);
};

MachineDivergenceAnalysis::MachineDivergenceAnalysis(
    MachineFunction &MF, const GASSInstrInfo *TII) 
      : TII(TII) {
  MRI = &MF.getRegInfo();
  // Mark source of divergence
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      // if source of divergence
      unsigned Opc = MI.getOpcode();
      if (Opc == GASS::READ_TID_X || Opc == GASS::READ_TID_Y || Opc == GASS::READ_TID_Z ||
          Opc == GASS::LDS16r || Opc == GASS::LDS16ri ||
          Opc == GASS::LDS32r || Opc == GASS::LDS32ri ||
          Opc == GASS::LDS64r || Opc == GASS::LDS64ri ||
          Opc == GASS::LDS128r || Opc == GASS::LDS128ri ||
          Opc == GASS::LDSM_x4_ri || Opc == GASS::LDSM_x4_rui ||
          Opc == GASS::LDSM_x4_ri_pseudo || Opc == TargetOpcode::IMPLICIT_DEF ||
          TII->isLDG(MI)) {
        for (MachineOperand &MO : MI.defs())
          if (MO.isReg()) {
            assert(MRI->hasOneDef(MO.getReg()));
            DivergentRegs.insert(MO.getReg());
          }
      }
    }
  } // for each MBB

  compute();
}

void MachineDivergenceAnalysis::compute() {
  DenseSet<Register> InitDiv = DivergentRegs;
  for (Register DivReg : InitDiv)
    pushUsers(DivReg);

  // All values on the Worklist are divergent.
  // Their users may not have been updated yet.
  while (!Worklist.empty()) {
    Register DivReg = Worklist.back();
    Worklist.pop_back();

    assert(isDivergent(DivReg));
    pushUsers(DivReg);
  }
}

void MachineDivergenceAnalysis::pushUsers(Register DivReg) {
  for (MachineInstr &MI : MRI->use_instructions(DivReg)) {
    for (MachineOperand &MO : MI.defs()) {
      if (MO.isReg()) {
        Register Reg = MO.getReg();
        assert(MRI->hasOneDef(Reg));
        if (!isDivergent(Reg)) {
          DivergentRegs.insert(Reg);
          Worklist.push_back(Reg);
        }
      }
    }
  }
}
//=--------------------------------------------------------------=//
// End of MachineDivergenceAnalysis
//=--------------------------------------------------------------=//

namespace {
class GASSMachineInstrCombine : public MachineFunctionPass {
  MachineRegisterInfo *MRI = nullptr;
  const GASSSubtarget *Subtarget = nullptr;
  const GASSRegisterInfo *TRI = nullptr;
  const GASSInstrInfo *TII = nullptr;
public:
  static char ID;

  GASSMachineInstrCombine() : MachineFunctionPass(ID) {
    initializeGASSMachineInstrCombinePass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return CPASS_NAME;
  }
private:
  DenseMap<Register, Register> VReg2SReg;
  bool runUniformRegisterCombine(MachineFunction &MF);
  bool doUniformRegisterCombine(MachineFunction &MF, MachineDivergenceAnalysis &MDA);
  // This version relies on MachineDivergenceAnalysis
  bool doUniformInstrTransform(MachineFunction &MF, MachineDivergenceAnalysis &MDA);
private:
  bool runZeroRegisterPropagate(MachineFunction &MF);
};
} // anonymous namespace

char GASSMachineInstrCombine::ID = 0;

INITIALIZE_PASS(GASSMachineInstrCombine, DEBUG_TYPE, CPASS_NAME, false, false)

bool GASSMachineInstrCombine::runOnMachineFunction(MachineFunction &MF) {
  bool MadeChange = false;
  MRI = &MF.getRegInfo();
  Subtarget = &MF.getSubtarget<GASSSubtarget>();
  TRI = Subtarget->getRegisterInfo();
  TII = Subtarget->getInstrInfo();

  if (!MRI->isSSA()) // Should we report error here?
    llvm_unreachable("Should be in SSA form");

  if (Subtarget->getSmVersion() <= 75)
    return false;

  MadeChange |= runUniformRegisterCombine(MF);

  return MadeChange;
}

static void simpleCSE(MachineFunction &MF, MachineRegisterInfo *MRI, 
                      const GASSInstrInfo *TII,
                      DenseSet<Register> &ActiveRegs) {
  // d0 = IADDrr sreg32:%a, sreg32:%b
  // d1 = IADD3rrr sreg32:%a, sreg32:%b, c
  //   =>
  // sreg:%d0 = IADDrr %a, %b
  // vreg:%d1 = IADDrr %c, %d0

  // within basicblock
  DenseSet<MachineInstr*> IADDrrCand;
  std::vector<MachineInstr*> ToDelete;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() == GASS::IADDrr) {
        Register a = MI.getOperand(1).getReg();
        Register b = MI.getOperand(2).getReg();
        if (ActiveRegs.contains(a) && ActiveRegs.contains(b)) {
          IADDrrCand.insert(&MI);
        }
      }

      if (MI.getOpcode() == GASS::IADD3rrr) {
        Register Dst = MI.getOperand(0).getReg();
        Register a = MI.getOperand(1).getReg();
        Register b = MI.getOperand(2).getReg();
        Register c = MI.getOperand(3).getReg();
        MachineOperand &PredImm = MI.getOperand(4);
        MachineOperand &Pred = MI.getOperand(5);

        if (ActiveRegs.contains(a) && ActiveRegs.contains(b) && !ActiveRegs.contains(c)) {
          for (MachineInstr *Cand : IADDrrCand) {
            if (Cand->getOperand(1).getReg() == a &&
                Cand->getOperand(2).getReg() == b) {
              Register NewDst = MRI->createVirtualRegister(&GASS::VReg32RegClass);
              BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(GASS::IADDrr), NewDst)
                .addReg(c).addReg(Cand->getOperand(0).getReg()).add(PredImm).add(Pred);
              MRI->replaceRegWith(Dst, NewDst);
              ToDelete.push_back(&MI);
            }
          }
        }
      }
    }
  } // for MBB : MF

  for (MachineInstr *MI : ToDelete)
    MI->removeFromParent();
}

static void replaceWithSReg(MachineFunction &MF, DenseSet<Register> &UniformRegs,
                            const GASSInstrInfo *TII,
                            MachineRegisterInfo *MRI) {
  std::vector<MachineInstr*> ToDelete;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.getNumDefs() != 1)
        continue;
      Register DefReg = MI.getOperand(0).getReg();
      if (UniformRegs.contains(DefReg)) {
        unsigned Opc = MI.getOpcode();
        const DebugLoc &DL = MI.getDebugLoc();
        const TargetRegisterClass *RC = nullptr;
        if (MRI->getRegClass(DefReg) == &GASS::VReg32RegClass)
          RC = &GASS::SReg32RegClass;
        else if(MRI->getRegClass(DefReg) == &GASS::VReg1RegClass)
          RC = &GASS::SReg1RegClass;
        else
          llvm_unreachable("error");

        Register NewDefReg = MRI->createVirtualRegister(RC);
        switch (Opc) {
        default:
          MF.dump(); MI.dump();
          llvm_unreachable("Cannot convert to U-instr");
        #define OP0Sub(ORIGIN, NEW) { \
        case ORIGIN: \
          BuildMI(MBB, MI, DL, TII->get(NEW), NewDefReg) \
            .add(MI.getOperand(1)).addReg(GASS::UPT); \
          break; \
        }
        #define OP1Sub(ORIGIN, NEW) { \
        case ORIGIN: \
          BuildMI(MBB, MI, DL, TII->get(NEW), NewDefReg) \
            .add(MI.getOperand(1)).add(MI.getOperand(2)).addReg(GASS::UPT); \
          break; \
        }
        #define OP2Sub(ORIGIN, NEW) { \
        case ORIGIN: \
          BuildMI(MBB, MI, DL, TII->get(NEW), NewDefReg) \
            .add(MI.getOperand(1)).add(MI.getOperand(2)).add(MI.getOperand(3)) \
            .addReg(GASS::UPT); \
          break; \
        }
        #define OP3Sub(ORIGIN, NEW) { \
        case ORIGIN: \
          BuildMI(MBB, MI, DL, TII->get(NEW), NewDefReg) \
            .add(MI.getOperand(1)).add(MI.getOperand(2)).add(MI.getOperand(3)) \
            .add(MI.getOperand(4)).addReg(GASS::UPT); \
          break; \
        }
        #define OP4Sub(ORIGIN, NEW) { \
        case ORIGIN: \
          BuildMI(MBB, MI, DL, TII->get(NEW), NewDefReg) \
            .add(MI.getOperand(1)).add(MI.getOperand(2)).add(MI.getOperand(3)) \
            .add(MI.getOperand(4)).add(MI.getOperand(5)).addReg(GASS::UPT); \
          break; \
        }

        OP0Sub(GASS::READ_CTAID_X, GASS::UREAD_CTAID_X);
        OP0Sub(GASS::READ_CTAID_Y, GASS::UREAD_CTAID_Y);
        OP0Sub(GASS::READ_CTAID_Z, GASS::UREAD_CTAID_Z);
        OP1Sub(GASS::LDC32c, GASS::ULDC32c);
        OP1Sub(GASS::MOV32r, GASS::UMOV32r);
        OP1Sub(GASS::MOV32i, GASS::UMOV32i);
        OP2Sub(GASS::SHL32ri, GASS::USHL32ri);
        OP2Sub(GASS::SRA32ri, GASS::USRA32ri);
        OP2Sub(GASS::SRL32ri, GASS::USRL32ri);
        OP2Sub(GASS::AND32ri, GASS::UAND32ri);
        OP2Sub(GASS::IADDrr, GASS::UIADDrr);
        OP2Sub(GASS::IADDri, GASS::UIADDri);
        OP2Sub(GASS::SUBrr, GASS::USUBrr);
        OP4Sub(GASS::ISETPri, GASS::UISETPri);
        // With out predicate
        case TargetOpcode::PHI: {
          BuildMI(MBB, MI, DL, TII->get(TargetOpcode::PHI), NewDefReg)
            .add(MI.getOperand(1)).add(MI.getOperand(2))
            .add(MI.getOperand(3)).add(MI.getOperand(4));
        } break;
        }
        ToDelete.push_back(&MI);
        MRI->replaceRegWith(DefReg, NewDefReg);
      }
    }
  } // For each MBB

  for (MachineInstr *MI : ToDelete)
    MI->removeFromParent();
}

// 1. change opcode / insert COPY if necessary
static void postProcess(MachineFunction &MF, const GASSInstrInfo *TII, 
                        MachineRegisterInfo *MRI) {
  std::vector<MachineInstr*> ToDelete;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() == GASS::IADDrr && 
          MRI->getRegClass(MI.getOperand(1).getReg()) == &GASS::VReg32RegClass &&
          MRI->getRegClass(MI.getOperand(2).getReg()) == &GASS::SReg32RegClass)
        MI.setDesc(TII->get(GASS::IADDru));
      // reorder ...
      if (MI.getOpcode() == GASS::IADDrr && 
          MRI->getRegClass(MI.getOperand(1).getReg()) == &GASS::SReg32RegClass &&
          MRI->getRegClass(MI.getOperand(2).getReg()) == &GASS::VReg32RegClass) {
        Register NewDst = MRI->createVirtualRegister(&GASS::VReg32RegClass);
        BuildMI(MBB, MI, MI.getDebugLoc(), TII->get(GASS::IADDru), NewDst)
          .add(MI.getOperand(2)).add(MI.getOperand(1))
          .add(MI.getOperand(3)).add(MI.getOperand(4));
        MRI->replaceRegWith(MI.getOperand(0).getReg(), NewDst);
        ToDelete.push_back(&MI);
      }
      // if (MI.getOpcode() == GASS::CBRA &&
      //     MRI->getRegClass(MI.getOperand(1).getReg()) == &GASS::SReg1RegClass)
      //   MI.setDesc(TII->get(GASS::UCBRA));
    }
  }

  for (MachineInstr *MI : ToDelete)
    MI->removeFromParent();
}

bool GASSMachineInstrCombine::runUniformRegisterCombine(MachineFunction &MF) {
  bool MadeChange = false;

  MachineDivergenceAnalysis MDA(MF, TII);

  // Collect activate vregs
  DenseSet<Register> ActiveRegs;
  for (MachineBasicBlock &MBB : MF)
  for (MachineInstr &MI : MBB)
    for (MachineOperand &MO : MI.operands())
      if (MO.isReg() && MO.getReg().isVirtual() && MDA.isUniform(MO.getReg()))
        ActiveRegs.insert(MO.getReg());

  simpleCSE(MF, MRI, TII, ActiveRegs);
  // 1. prune by def
  DenseSet<Register> UniformCands;
  for (Register Reg : ActiveRegs) {
    MachineInstr *DefMI = MRI->getVRegDef(Reg);
    assert(DefMI);
    unsigned Opc = DefMI->getOpcode();
    if (Opc == GASS::MOV32f)
      continue; // Float cannot be sreg
    UniformCands.insert(Reg);
  }
  // 2. prune by use 
  DenseSet<Register> UniformCands2;
  for (Register Reg : UniformCands) {
    bool ShouldAdd = true;
    for (MachineInstr &UseMI : MRI->use_instructions(Reg)) {
      unsigned Opc = UseMI.getOpcode();
      if (Opc == GASS::CBRA) {
        dbgs() << "Remove %" << Register::virtReg2Index(Reg) << "\n";
        ShouldAdd = false;
        break;
      }
    }
    if (ShouldAdd)
      UniformCands2.insert(Reg);
  }

  // 3. prune recursively
  bool MadePrune = false;
  do {
    MadePrune = false;
    for (MachineBasicBlock &MBB : MF) {
      for (MachineInstr &MI : MBB) {
        // 
      }
    }
  } while (MadePrune);

  for (Register Reg : UniformCands2)
  if (MDA.isUniform(Reg))
    LLVM_DEBUG(dbgs() << "%" << Register::virtReg2Index(Reg) << "\n");

  replaceWithSReg(MF, UniformCands2, TII, MRI);
  postProcess(MF, TII, MRI);
  MadeChange = doUniformRegisterCombine(MF, MDA);
  while(MadeChange && doUniformRegisterCombine(MF, MDA));

  return MadeChange;
}

// Ideally, this should be part of the selection dag. But I don't know how to fit this
// into the SelectionDAG framework.
bool GASSMachineInstrCombine::doUniformRegisterCombine(MachineFunction &MF,
                                                       MachineDivergenceAnalysis &MDA) {
  bool MadeChange = false;

  std::vector<MachineInstr*> ToDelete;
  // Forward propagate 
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      unsigned Opc = MI.getOpcode();
      const DebugLoc &DL = MI.getDebugLoc();
      switch (Opc) {
      default: break;
      // case GASS::IADDrr: { // -> IADDru | UIADDuu
      //   Register Dst = MI.getOperand(0).getReg();
      //   Register Op0Reg = MI.getOperand(1).getReg();
      //   Register Op1Reg = MI.getOperand(2).getReg();
      //   MachineOperand &PredImm = MI.getOperand(3);
      //   MachineOperand &Pred = MI.getOperand(4);

      //   bool IsOp0SReg = VReg2SReg.find(Op0Reg) != VReg2SReg.end();
      //   bool IsOp1SReg = VReg2SReg.find(Op1Reg) != VReg2SReg.end();
      //   Register NewDst = MRI->createVirtualRegister(&GASS::VReg32RegClass);
      //   if (IsOp0SReg && !IsOp1SReg) {
      //     BuildMI(MBB, MI, DL, TII->get(GASS::IADDru), NewDst)
      //       .addReg(Op1Reg).addReg(VReg2SReg[Op0Reg]).add(PredImm).add(Pred);
      //     MRI->replaceRegWith(Dst, NewDst);
      //     ToDelete.push_back(&MI);
      //   } else if (!IsOp0SReg && IsOp1SReg) {
      //     BuildMI(MBB, MI, DL, TII->get(GASS::IADDru), NewDst)
      //       .addReg(Op0Reg).addReg(VReg2SReg[Op1Reg]).add(PredImm).add(Pred);
      //     MRI->replaceRegWith(Dst, NewDst);
      //     ToDelete.push_back(&MI);
      //   } else if(IsOp0SReg && IsOp1SReg) { // UIADDuu
      //     // TODO: add support for it.
      //     // llvm_unreachable("Not implemented");
      //   }
      // } break;
      case GASS::LDSM_x4_ri_pseudo: { // -> LDSM_x4_rui_pseudo if possible
        Register D0 = MI.getOperand(0).getReg();
        Register D1 = MI.getOperand(1).getReg();
        Register D2 = MI.getOperand(2).getReg();
        Register D3 = MI.getOperand(3).getReg();
        Register Ptr = MI.getOperand(4).getReg();
        MachineOperand &IOff = MI.getOperand(5);
        MachineOperand &Trans = MI.getOperand(6);

        MachineOperand *PtrDef = MRI->getOneDef(Ptr);
        if (!PtrDef) {
          break;
        }
        MachineInstr *PtrDefMI = PtrDef->getParent();

        // FIXME: IADDru is not pseudo instr. It has Predicate masks. But will be ignored
        if (PtrDefMI->getOpcode() == GASS::IADDru) {
          MachineOperand &VOff = PtrDefMI->getOperand(1);
          MachineOperand &SOff = PtrDefMI->getOperand(2);
          Register NewD0 = MRI->createVirtualRegister(&GASS::VReg32RegClass);
          Register NewD1 = MRI->createVirtualRegister(&GASS::VReg32RegClass);
          Register NewD2 = MRI->createVirtualRegister(&GASS::VReg32RegClass);
          Register NewD3 = MRI->createVirtualRegister(&GASS::VReg32RegClass);

          BuildMI(MBB, MI, DL, TII->get(GASS::LDSM_x4_rui_pseudo))
            .addReg(NewD0, RegState::Define).addReg(NewD1, RegState::Define)
            .addReg(NewD2, RegState::Define).addReg(NewD3, RegState::Define)
            .add(VOff).add(SOff).add(IOff).add(Trans);
          MRI->replaceRegWith(D0, NewD0);
          MRI->replaceRegWith(D1, NewD1);
          MRI->replaceRegWith(D2, NewD2);
          MRI->replaceRegWith(D3, NewD3);
          
          ToDelete.push_back(&MI);
        }
      } break;
      // TODO: STS also...
      }
    } // for each MI
  } // for each MBB

  for (MachineInstr *MI : ToDelete) {
    MadeChange = true;
    MI->eraseFromParent();
  }
  return MadeChange;
}

// bool GASSMachineInstrCombine::canReplaceWithUR(MachineInstr &MI, Register Reg, 
//                                                MachineDivergenceAnalysis &MDA) {
//   unsigned Opc = MI.getOpcode();
//   // 1. Check if can replace def instr
//   switch (Opc) {
//   default: return false;
//   case GASS::MOV32i: return true;
//   }

//   // 2. Check if uses can accept. 
// }

// bool GASSMachineInstrCombine::replaceWithUR(MachineInstr &MI, Register DefReg) {
//   // rules for different instrs
//   // 1. replace the def instr
//   unsigned Opc = MI.getOpcode();
//   MachineBasicBlock &MBB = *MI.getParent();
//   const DebugLoc &DL = MI.getDebugLoc();

//   TargetRegisterClass *NewRegClass = nullptr;
//   if (MRI->getRegClass(DefReg) == &GASS::VReg32RegClass)
//     NewRegClass = &GASS::SReg32RegClass;
//   else if(MRI->getRegClass(DefRef) == &GASS::VReg1RegClass)
//     NewRegClass = &GASS::SReg1RegClass;
//   else
//     llvm_unreachable("Reg Class not supported");
//   Register NewDefReg = MRI->createVirtualRegister(NewRegClass);
//   switch (Opc) {
//   case GASS::MOV32i: {
//     BuildMI(MBB, MI, DL, TII->get(GASS::UMOV32), NewDefReg)
//       .add(MI.getOperand(1)).add(MI.getOperand(2)).addReg(GASS::UPT);
//     MRI->replaceRegWith(DefReg, NewDefReg);
//   }
//   }

//   // 2. replace all uses
//   for (MachineOperand &Use : MRI->use_operands(NewDefReg)) {
//     MachineInstr &UseMI = *Use.getParent();
//     unsigned Opc = UseMI.getOpcode();
//     switch (Opc) {
//     default: return false;
//     case TargetOpcode::PHI: {
//       assert(MDA.isUniform(UseMI.defs().begin()->getReg()));
//       return true;
//     } break;
//     // Instruction that can accept ur
//     case GASS::IADDrr: {
//       // swap oprands if necessary
//       if (&UseMI.getOperand(1) == &Use) {
//         MRI->moveOperands(&UseMI.getOperand(1), &UseMI.getOperand(2), 1);
//       }
//       assert(&UseMI.getOperand(2) == &Use);
//       UseMI.setDesc(TII->get(GASS::IADDru));
//     } break;
//     // Trigers new replacement
//     case GASS::IADDru: // case GASS::SHF
//       return true;
//     }
//   }
// }

bool GASSMachineInstrCombine::doUniformInstrTransform(MachineFunction &MF, 
                                                      MachineDivergenceAnalysis &MDA) {
  // // 
  // for (MachineBasicBlock &MBB : MF) {
  //   for (MachineInstr &MI : MBB) {
  //     // We only care about instr with single def (why?)
  //     if (!MI.getNumDefs() == 1)
  //       continue;
  //     MachineOperand &Def = *MI.defs().begin();
  //     // Only care about virtual regs
  //     if (!Def.isReg() || !Def.getReg().isVirtual())
  //       continue;
  //     Register DefReg = Def.getReg();
  //     if (MDA.isUniform(DefReg)) {
  //       // Can all users use sregs? If not, keeps vregs
  //       if (canReplaceWithUR(MI, DefReg, MDA)) {
  //         replaceWithUR(MI, );
  //       }
  //     }
  //   }
  // }

  // verify result
}

//=--------------------------------------------------------------------------------=//
// Zero-Register Propagate
//=--------------------------------------------------------------------------------=//
// I'm too lazy to write a new pass :)
// This pass Replace Imm/FPImm 0 with RZ
bool GASSMachineInstrCombine::runZeroRegisterPropagate(MachineFunction &MF) {
  bool MadeChange = false;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      unsigned Opc = MI.getOpcode();
      const DebugLoc &DL = MI.getDebugLoc();
      switch (Opc) {
      default: break; // Do nothing
      case GASS::MOV32i: {
        if (MI.getOperand(1).getImm() == 0) {
          Register Dst = MI.getOperand(0).getReg();
          MRI->replaceRegWith(Dst, GASS::RZ32);
          MadeChange = true;
        }
      } break;
      }
    }
  }
  return MadeChange;
}

FunctionPass *llvm::createGASSMachineInstrCombinePass() {
  return new GASSMachineInstrCombine();
}