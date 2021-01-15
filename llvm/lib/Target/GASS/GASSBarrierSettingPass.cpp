#include "GASS.h"
#include "GASSInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

using namespace llvm;

#define DEBUG_TYPE "gass-barrier-setting"

#define GASS_BARRIERSETTING_NAME "Setting Instruction Wait Barriers"

namespace {
class GASSBarrierSetting : public MachineFunctionPass {
public:
  static char ID;

  GASSBarrierSetting() : MachineFunctionPass(ID) {
    initializeGASSBarrierSettingPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return GASS_BARRIERSETTING_NAME;
  }
};

char GASSBarrierSetting::ID = 0;
} // anonymous 

namespace llvm {
  void initializeMachineDominatorTreePass(PassRegistry&);
  void initializeMachineDominanceFrontierPass(PassRegistry&);
}

INITIALIZE_PASS_BEGIN(GASSBarrierSetting, DEBUG_TYPE,
                      GASS_BARRIERSETTING_NAME, false, false);
INITIALIZE_PASS_DEPENDENCY(MachineDominatorTree)
INITIALIZE_PASS_DEPENDENCY(MachineDominanceFrontier)
INITIALIZE_PASS_END(GASSBarrierSetting, DEBUG_TYPE,
                    GASS_BARRIERSETTING_NAME, false, false);

bool GASSBarrierSetting::runOnMachineFunction(MachineFunction &MF) {
  const TargetSubtargetInfo &ST = MF.getSubtarget();
  const TargetInstrInfo *TII = ST.getInstrInfo();
  const TargetRegisterInfo *TRI = ST.getRegisterInfo();

  // Use MachineRegisterInfo
  const MachineRegisterInfo &MRI = MF.getRegInfo();
  for (const MachineBasicBlock &MBB : MF) {
    for (const MachineInstr &MI : MBB) {
      for (const MachineOperand &MOP : MI.operands()) {
        if (MOP.isReg()) {
          MOP.dump();
          errs() << " in ";
          MI.dump();
          errs() << " is used by ";
          for (const MachineOperand &X : MRI.use_operands(MOP.getReg())) {
            X.dump();
            errs() << " in ";
            X.getParent()->dump();
          }
        }
      }
    }
  }

  return true;
}

FunctionPass *llvm::createGASSBarrierSettingPass() {
  return new GASSBarrierSetting();
}