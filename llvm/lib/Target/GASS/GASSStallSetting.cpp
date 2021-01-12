#include "GASS.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/TargetSchedule.h"

using namespace llvm;

#define DEBUG_TYPE "gass-stall-setting"

// This pass sets stall cycles for each instructions
namespace {
class GASSStallSetting : public MachineFunctionPass {
  TargetSchedModel SchedModel;

public:
  static char ID;

  GASSStallSetting() : MachineFunctionPass(ID) {
    initializeGASSStallSettingPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return "Setting Instruction Stall Cycles";
  }
};

char GASSStallSetting::ID = 0;
} // anonymous namespace

INITIALIZE_PASS(GASSStallSetting, "gass-stall-setting",
                "Setting Instruction Stall Cycles", false, false);


bool GASSStallSetting::runOnMachineFunction(MachineFunction &MF) {
  const TargetSubtargetInfo &ST = MF.getSubtarget();
  SchedModel.init(&ST);
  if (!SchedModel.hasInstrSchedModel())
    return false;

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      unsigned Lat = SchedModel.computeInstrLatency(&MI, false);
      MI.dump();
      printf("'s Lat is %d\n", Lat);
    }
  }
}

FunctionPass *llvm::createGASSStallSettingPass() {
  return new GASSStallSetting();
}