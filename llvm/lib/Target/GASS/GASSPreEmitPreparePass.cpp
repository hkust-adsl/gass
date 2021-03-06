#include "GASS.h"
#include "MCTargetDesc/GASSMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
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
  std::vector<MachineInstr*> ToDeletInstrs;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineBasicBlock::iterator MII = MBB.begin();
                                     MII != MBB.end(); ++MII) {
      MachineInstr &MI = *MII;
      if (MI.getOpcode() == TargetOpcode::IMPLICIT_DEF) {
        DebugLoc DL = MI.getDebugLoc();
        
        BuildMI(MBB, MI, DL, TII->get(GASS::NOP))
          .addReg(GASS::PT);
        
        ToDeletInstrs.push_back(&MI);
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