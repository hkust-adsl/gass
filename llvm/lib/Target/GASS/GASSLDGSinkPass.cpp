#include "GASS.h"
#include "GASSInstrInfo.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "gass-ldg-sink"
#define CPASS_NAME "GASS LDG Sink"

namespace {
class GASSLDGSink : public MachineFunctionPass {
  const GASSInstrInfo *TII = nullptr;
  const MachineRegisterInfo *MRI = nullptr;
public:
  static char ID;

  GASSLDGSink() : MachineFunctionPass(ID) {
    initializeGASSLDGSinkPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  bool runOnLoopBody(MachineBasicBlock &MBB);

  StringRef getPassName() const override {
    return CPASS_NAME;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineLoopInfo>();
    AU.addPreserved<MachineLoopInfo>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};
} // anonymous namespace

char GASSLDGSink::ID = 0;

INITIALIZE_PASS(GASSLDGSink, DEBUG_TYPE, CPASS_NAME, false, false)

bool GASSLDGSink::runOnLoopBody(MachineBasicBlock &MBB) {
  bool MadeChange = false;

  // 1. Build Def-Use Chain
  // buildDefUseChain();

  // LDG -> instr that use the result of LDGs
  DenseMap<MachineInstr *, MachineInstr *> LDGs;
  for (MachineInstr &MI : MBB) {
    // 1. Collect LDGs & target operands
    llvm_unreachable("Not implemented");
    // Ref: ScheduleDAGInstrs::addPhysRegDeps(SU, OperIdx)
    if (TII->isLDG(MI)) {
      assert(MI.getNumDefs() == 1);
      MachineOperand *LDGDst = &MI.getOperand(0);
      assert(LDGDst->isDef());
      // LDGs[&MI] = ;
    }
  }


  // 2. Compute Free LDST slot
  llvm_unreachable("Not implemented");

  // 3. Try to sink LDGs
  llvm_unreachable("Not implemented");

  return MadeChange;
}

bool GASSLDGSink::runOnMachineFunction(MachineFunction &MF) {
  const MachineLoopInfo *Loops = &getAnalysis<MachineLoopInfo>();
  bool MadeChange = false;

  TII = static_cast<const GASSInstrInfo*>(MF.getSubtarget().getInstrInfo());
  MRI = &MF.getRegInfo();

  for (MachineBasicBlock &MBB : MF) {
    // We only care about loops
    if (Loops->getLoopDepth(&MBB) == 0)
      continue;

    MadeChange |= runOnLoopBody(MBB);    
  }

  return MadeChange;
}

//------------------------- Public Interface -----------------------------------
FunctionPass *llvm::createGASSLDGSinkPass() {
  return new GASSLDGSink();
}