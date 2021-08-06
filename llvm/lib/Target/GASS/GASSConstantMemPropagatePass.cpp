#include "GASS.h"
#include "GASSInstrInfo.h"
#include "GASSRegisterInfo.h"
#include "GASSSubtarget.h"
#include "MCTargetDesc/GASSMCTargetDesc.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachinePostDominators.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

using namespace llvm;

#define DEBUG_TYPE "gass-constant-mem-propagate"
#define CPASS_NAME "GASS Constant Memory Propagate"

namespace {
class GASSConstantMemPropagate : public MachineFunctionPass {
  MachineRegisterInfo *MRI = nullptr;
  const GASSSubtarget *Subtarget = nullptr;
  const GASSRegisterInfo *TRI = nullptr;
  const GASSInstrInfo *TII = nullptr;
public:
  static char ID;

  GASSConstantMemPropagate() : MachineFunctionPass(ID) {
    initializeGASSConstantMemPropagatePass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return CPASS_NAME;
  }
private:
  // VReg -> constmem offset
  DenseMap<Register, unsigned> Reg2ConstMem;
};
} // anonymous namespace

char GASSConstantMemPropagate::ID = 0;

INITIALIZE_PASS(GASSConstantMemPropagate, DEBUG_TYPE, CPASS_NAME, false, false)

bool GASSConstantMemPropagate::runOnMachineFunction(MachineFunction &MF) {
  bool MadeChange = false;
  MRI = &MF.getRegInfo();
  Subtarget = &MF.getSubtarget<GASSSubtarget>();
  TRI = Subtarget->getRegisterInfo();
  TII = Subtarget->getInstrInfo();

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
        assert(MI.getNumOperands() == 7);
        Register Dst = MI.getOperand(0).getReg();
        MachineOperand &Op0 = MI.getOperand(1);
        Register Op1Reg = MI.getOperand(2).getReg();
        MachineOperand &CmpMode = MI.getOperand(3);
        MachineOperand &CmpModeSign = MI.getOperand(4);
        MachineOperand &PredImm = MI.getOperand(5);
        MachineOperand &Pred = MI.getOperand(6);

        Register NewDst = MRI->createVirtualRegister(&GASS::VReg1RegClass);
        if (Reg2ConstMem.find(Op1Reg) != Reg2ConstMem.end()) {
          BuildMI(MBB, MI, DL, TII->get(GASS::ISETPrc), NewDst)
            .add(Op0).addImm(Reg2ConstMem[Op1Reg]).add(CmpMode).add(CmpModeSign)
            .add(PredImm).add(Pred);
          MRI->replaceRegWith(Dst, NewDst);
          ToDelete.push_back(&MI);
        }
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

FunctionPass *llvm::createGASSConstantMemPropagatePass() {
  return new GASSConstantMemPropagate();
}