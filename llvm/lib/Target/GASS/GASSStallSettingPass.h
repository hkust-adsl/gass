#include "GASSTargetMachine.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/TargetSchedule.h"


namespace llvm {
class MachineFunction;
class TargetSchedModel;

class GASSStallSetting : public MachineFunctionPass {
  TargetSchedModel SchedModel;
  DenseMap<MachineInstr *, unsigned> StallCycleMap;
public:
  static char ID;

  GASSStallSetting() : MachineFunctionPass(ID) {
    initializeGASSStallSettingPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  /// Query result
  unsigned getStallCycles(const MachineInstr *MI) const {
    return StallCycleMap.lookup(MI);
  }

  StringRef getPassName() const override {
    return "Setting Instruction Stall Cycles";
  }
};
}