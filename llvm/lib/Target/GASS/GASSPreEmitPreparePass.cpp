#include "GASS.h"
#include "GASSSubtarget.h"
#include "MCTargetDesc/GASSMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/PassRegistry.h"
using namespace llvm;

#define DEBUG_TYPE "gass-pre-emit-prepare"
#define CPASS_NAME "GASS Pre-emit prepare"

namespace {
class GASSPreEmitPrepare : public MachineFunctionPass {
public:
  static char ID;

  GASSPreEmitPrepare() : MachineFunctionPass(ID) {
    initializeGASSPreEmitPreparePass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return CPASS_NAME;
  }
};
} // anonymous namespace

char GASSPreEmitPrepare::ID = 0;

INITIALIZE_PASS(GASSPreEmitPrepare, DEBUG_TYPE, CPASS_NAME, false, false)

bool GASSPreEmitPrepare::runOnMachineFunction(MachineFunction &MF) {
  const TargetInstrInfo *TII = MF.getSubtarget().getInstrInfo();
  const GASSSubtarget *Subtarget = 
      static_cast<const GASSSubtarget*>(&MF.getSubtarget()); 
  std::vector<MachineInstr*> ToDeletInstrs;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineBasicBlock::iterator MII = MBB.begin();
                                     MII != MBB.end(); ++MII) {
      MachineInstr &MI = *MII;
      // Replace IMPLICIT_DEF & KILL with NOP
      // TODO: Or just delete them?
      if (MI.getOpcode() == TargetOpcode::IMPLICIT_DEF || 
          MI.getOpcode() == TargetOpcode::KILL) {
        DebugLoc DL = MI.getDebugLoc();
        
        BuildMI(MBB, MI, DL, TII->get(GASS::NOP))
          .addImm(0).addReg(GASS::PT);
        
        ToDeletInstrs.push_back(&MI);
      }
      // Replace BAR.SYNC with BAR.SYNC.DEFER_BLOCKING on *Volta* GPUs
      // in loop body (?)
      if (MI.getOpcode() == GASS::BAR) {
        if (Subtarget->getSmVersion() == 70) {
          assert(MI.getNumOperands() == 2 &&
                MI.getOperand(0).isImm() && MI.getOperand(1).isReg());
          DebugLoc DL = MI.getDebugLoc();
          BuildMI(MBB, MI, DL, TII->get(GASS::BAR_DEFER))
            .add(MI.getOperand(0))
            .add(MI.getOperand(1));

          ToDeletInstrs.push_back(&MI);
        }
      }
    }
  }

  // erase deleted MIs
  for (MachineInstr *MI : ToDeletInstrs) {
    MI->eraseFromParent();
  }
}

FunctionPass *llvm::createGASSPreEmitPreparePass() {
  return new GASSPreEmitPrepare();
}