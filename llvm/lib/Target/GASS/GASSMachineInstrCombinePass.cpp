#include "GASS.h"
#include "GASSInstrInfo.h"
#include "GASSRegisterInfo.h"
#include "GASSSubtarget.h"
#include "MCTargetDesc/GASSMCTargetDesc.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

using namespace llvm;

#define DEBUG_TYPE "gass-machine-instr-combine"
#define CPASS_NAME "GASS MachineInstr Combine"

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
  // VReg -> constmem offset
  DenseMap<Register, unsigned> Reg2ConstMem;
  bool runConstantMemoryPropagate(MachineFunction &MF);
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

  if (Subtarget->getSmVersion() < 75)
    return false;

  // MadeChange |= runZeroRegisterPropagate(MF);

  // std::vector<MachineInstr*> ToDelete;
  // for (MachineBasicBlock &MBB : MF) {
  //   for (MachineInstr &MI : MBB) {
  //     unsigned Opc = MI.getOpcode();
  //     if (Opc == TargetOpcode::COPY) {
  //       assert(MI.getNumExplicitDefs() == 1);
  //       MachineOperand &Dst = MI.getOperand(0);
  //       MachineOperand &Src = MI.getOperand(1);
  //       assert(Dst.isReg() && Dst.isDef() && Src.isReg() && Src.isUse());
  //       // MI.dump();
  //       const TargetRegisterClass *DstRC = MRI->getRegClass(Dst.getReg());
  //       const TargetRegisterClass *SrcRC = MRI->getRegClass(Src.getReg());
  //       if (DstRC == &GASS::VReg32RegClass && SrcRC == &GASS::SReg32RegClass) {
  //         if (MRI->hasOneUse(Dst.getReg())) { // replace vreg with sreg is possible
  //           MachineInstr *TargetMI = MRI->use_begin(Dst.getReg())->getParent();
  //           const DebugLoc &DL = TargetMI->getDebugLoc();
  //           if (TargetMI->getOpcode() == GASS::IADD3rrr) {
  //             MachineOperand &IADDDst = TargetMI->getOperand(0);
  //             MachineOperand &MO0 = TargetMI->getOperand(1);
  //             MachineOperand &MO1 = TargetMI->getOperand(2);
  //             MachineOperand &MO2 = TargetMI->getOperand(3);
  //             MachineOperand &PredImm = TargetMI->getOperand(4);
  //             MachineOperand &PredReg = TargetMI->getOperand(5);

  //             MachineOperand *NewMO0, *NewMO1, *NewMO2;

  //             if (MO0.getReg() == Dst.getReg()) {
  //               NewMO0 = &MO1;
  //               NewMO1 = &Src;
  //               NewMO2 = &MO2;
  //             } else if (MO1.getReg() == Dst.getReg()) {
  //               NewMO0 = &MO0;
  //               NewMO1 = &Src;
  //               NewMO2 = &MO2;
  //             } else if (MO2.getReg() == Dst.getReg()) {
  //               NewMO0 = &MO0;
  //               NewMO1 = &Src;
  //               NewMO2 = &MO1;
  //             } else
  //               llvm_unreachable("Shouldn't be here");

  //             Register NewAddDst = MRI->createVirtualRegister(&GASS::VReg32RegClass);              
  //             BuildMI(MBB, *TargetMI, DL, TII->get(GASS::IADD3rur), NewAddDst)
  //               .add(*NewMO0).add(*NewMO1).add(*NewMO2).add(PredImm).add(PredReg);
              
  //             MRI->replaceRegWith(IADDDst.getReg(), NewAddDst);
              
  //             ToDelete.push_back(TargetMI);
  //             ToDelete.push_back(&MI); // COPY
  //           }
  //         }
  //       } // if (DstRc == vreg32 && SrcRC == sreg32)
  //     }
  //   }
  // }

  // // erase deleted MIs
  // for (MachineInstr *MI : ToDelete) {
  //   MadeChange = true;
  //   MI->eraseFromParent();
  // }

  // ConstantMemory propagate here
  // TODO: move this to another file
  MadeChange |= runConstantMemoryPropagate(MF);

  return MadeChange;
}

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

// I'm too lazy to write a new pass :)
bool GASSMachineInstrCombine::runConstantMemoryPropagate(MachineFunction &MF) {
  bool MadeChange = false;
  // 1. collect constmem info
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      unsigned Opc = MI.getOpcode();
      const DebugLoc &DL = MI.getDebugLoc();
      if (Opc == GASS::LDC32c || Opc == GASS::LDC64c ||
          Opc == GASS::ULDC32c || Opc == GASS::ULDC64c) {
        Reg2ConstMem[MI.getOperand(0).getReg()] = MI.getOperand(1).getImm();
      } else if (MI.isCopy()) {
        Register Dst = MI.getOperand(0).getReg();
        Register Src = MI.getOperand(1).getReg();
        if (Reg2ConstMem.find(Src) != Reg2ConstMem.end())
          Reg2ConstMem[Dst] = Reg2ConstMem[Src];
      }
    }
  }

  // 2. Replace vreg with constant mem
  std::vector<MachineInstr*> ToDelete;
  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      unsigned Opc = MI.getOpcode();
      const DebugLoc &DL = MI.getDebugLoc();
      switch (Opc) {
      default: continue;
      case GASS::IADD64rr: {
        Register Dst = MI.getOperand(0).getReg();
        Register Op0Reg = MI.getOperand(1).getReg();
        Register Op1Reg = MI.getOperand(2).getReg();
        MachineOperand &PredImm = MI.getOperand(3);
        MachineOperand &Pred = MI.getOperand(4);

        Register NewDst = MRI->createVirtualRegister(&GASS::VReg64RegClass);
        if (Reg2ConstMem.find(Op0Reg) != Reg2ConstMem.end()) {
          BuildMI(MBB, MI, DL, TII->get(GASS::IADD64rc), NewDst)
            .addReg(Op1Reg).addImm(Reg2ConstMem[Op0Reg]).add(PredImm).add(Pred);
          MRI->replaceRegWith(Dst, NewDst);
          ToDelete.push_back(&MI);
        } else if (Reg2ConstMem.find(Op1Reg) != Reg2ConstMem.end()) {
          BuildMI(MBB, MI, DL, TII->get(GASS::IADD64rc), NewDst)
            .addReg(Op0Reg).addImm(Reg2ConstMem[Op1Reg]).add(PredImm).add(Pred);
          MRI->replaceRegWith(Dst, NewDst);
          ToDelete.push_back(&MI);
        }
      } break;
      case GASS::ISETPrr: {

      } break;
      case GASS::IMAD_S32rrr: {
        Register Dst = MI.getOperand(0).getReg();
        Register Op0Reg = MI.getOperand(1).getReg();
        Register Op1Reg = MI.getOperand(2).getReg();
        MachineOperand &Op2 = MI.getOperand(3);
        MachineOperand &PredImm = MI.getOperand(4);
        MachineOperand &Pred = MI.getOperand(5);

        Register NewDst = MRI->createVirtualRegister(&GASS::VReg32RegClass);
        if (Reg2ConstMem.find(Op0Reg) != Reg2ConstMem.end()) {
          BuildMI(MBB, MI, DL, TII->get(GASS::IMAD_S32rcr), NewDst)
            .addReg(Op1Reg).addImm(Reg2ConstMem[Op0Reg]).add(Op2).add(PredImm).add(Pred);
          MRI->replaceRegWith(Dst, NewDst);
          ToDelete.push_back(&MI);
        } else if (Reg2ConstMem.find(Op1Reg) != Reg2ConstMem.end()) {
          BuildMI(MBB, MI, DL, TII->get(GASS::IMAD_S32rcr), NewDst)
            .addReg(Op0Reg).addImm(Reg2ConstMem[Op1Reg]).add(Op2).add(PredImm).add(Pred);
          MRI->replaceRegWith(Dst, NewDst);
          ToDelete.push_back(&MI);
        }
      };
      }
    }
  } // for (MBB : MF)

  // erase deleted MIs
  for (MachineInstr *MI : ToDelete) {
    MadeChange = true;
    LLVM_DEBUG(MI->dump());
    MI->eraseFromParent();
  }

  return MadeChange;
}

FunctionPass *llvm::createGASSMachineInstrCombinePass() {
  return new GASSMachineInstrCombine();
}