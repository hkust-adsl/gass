#include "GASS.h"
#include "GASSInstrInfo.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define DEBUG_TYPE "gass-mark-undead"
#define CPASS_NAME "GASS Mark Undead"

namespace {
class GASSMarkUndead : public MachineFunctionPass {
public:
  static char ID;

  GASSMarkUndead() : MachineFunctionPass(ID) {
    initializeGASSMarkUndeadPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;
  bool runOnLoopBody(MachineBasicBlock &MBB);

  StringRef getPassName() const override {
    return CPASS_NAME;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LiveIntervals>();
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};
} // anonymous namespace

char GASSMarkUndead::ID = 0;

INITIALIZE_PASS_BEGIN(GASSMarkUndead, DEBUG_TYPE, CPASS_NAME, false, false)
INITIALIZE_PASS_DEPENDENCY(LiveIntervals)
INITIALIZE_PASS_END(GASSMarkUndead, DEBUG_TYPE, CPASS_NAME, false, false)


bool GASSMarkUndead::runOnMachineFunction(MachineFunction &MF) {
  bool MadeChange = false;

  auto *LIS = &getAnalysis<LiveIntervals>();

  for (MachineBasicBlock &MBB : MF) {
    for (MachineInstr &MI : MBB) {
      for (MachineOperand &MO : MI.defs()) {
        if (!MO.isReg())
          continue;
        Register Reg = MO.getReg();
        if (!Reg.isVirtual())
          continue;
        if (MO.isDead()) {
          MO.setIsDead(false);
          MadeChange = true;
        }
      }
    }
  }

  LLVM_DEBUG(LIS->dump());

  return MadeChange;
}

//------------------------- Public Interface -----------------------------------
FunctionPass *llvm::createGASSMarkUndeadPass() {
  return new GASSMarkUndead();
}