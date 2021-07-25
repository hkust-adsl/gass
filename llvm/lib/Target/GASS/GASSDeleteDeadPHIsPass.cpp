#include "GASS.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/MemorySSAUpdater.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/PassRegistry.h"

using namespace llvm;

#define DEBUG_TYPE "gass-delete-dead-phis"

namespace {
class GASSDeleteDeadPHIs : public FunctionPass {
public:
  static char ID;
  GASSDeleteDeadPHIs() : FunctionPass(ID) {
    initializeGASSDeleteDeadPHIsPass(*PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    FunctionPass::getAnalysisUsage(AU);
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    AU.addPreserved<MemorySSAWrapperPass>();
  }
};
} // anonymous namespace

bool GASSDeleteDeadPHIs::runOnFunction(Function &F) {
  auto *MSSAnalysis = getAnalysisIfAvailable<MemorySSAWrapperPass>();
  MemorySSA *MSSA = nullptr;
  if (MSSAnalysis)
    MSSA = &MSSAnalysis->getMSSA();
  std::unique_ptr<MemorySSAUpdater> MSSAU;
  if (MSSA)
    MSSAU = std::make_unique<MemorySSAUpdater>(MSSA);

  auto &TLI = getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(F);
  
  bool Changed = false;
  for (BasicBlock &BB : F)
    Changed |= DeleteDeadPHIs(&BB, &TLI, MSSAU.get());
  return Changed;
}

char GASSDeleteDeadPHIs::ID = 0;

INITIALIZE_PASS_BEGIN(GASSDeleteDeadPHIs, DEBUG_TYPE, "Delete Dead PHIs", false, false)
INITIALIZE_PASS_END(GASSDeleteDeadPHIs, DEBUG_TYPE, "Delete Dead PHIs", false, false)

FunctionPass *llvm::createGASSDeleteDeadPHIsPass() { return new GASSDeleteDeadPHIs(); }