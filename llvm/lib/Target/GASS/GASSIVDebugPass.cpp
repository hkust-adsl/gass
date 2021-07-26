#include "GASS.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/IVUsers.h"
#include "llvm/InitializePasses.h"
#include "llvm/IR/Dominators.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/Pass.h"

using namespace llvm;

#define DEBUG_TYPE "gass-iv-debug"

namespace {
class GASSIVDebug : public LoopPass {
public:
  static char ID;
  GASSIVDebug() : LoopPass(ID) {
    initializeGASSIVDebugPass(*PassRegistry::getPassRegistry());
  }

  bool runOnLoop(Loop *L, LPPassManager &LPM) override;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addPreserved<LoopInfoWrapperPass>();
    AU.addRequired<ScalarEvolutionWrapperPass>();
    AU.addPreserved<ScalarEvolutionWrapperPass>();
    AU.addRequired<IVUsersWrapperPass>();
    AU.addPreserved<IVUsersWrapperPass>();
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addPreserved<DominatorTreeWrapperPass>();
    AU.addRequired<LiveIntervals>();
    AU.addPreserved<LiveIntervals>();
  }
};
} // anonymous namespace

bool GASSIVDebug::runOnLoop(Loop *L, LPPassManager & /*LPM*/) {
  auto &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
  auto &SE = getAnalysis<ScalarEvolutionWrapperPass>().getSE();
  auto &IU = getAnalysis<IVUsersWrapperPass>().getIU();
  auto &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();

  // // Ref: LSRInstance::CollectChains()
  // SmallVector<BasicBlock *, 8> LatchPath;
  // BasicBlock *LoopHeader = L->getHeader();
  // for (DomTreeNode *Rung = DT.getNode(L->getLoopLatch());
  //      Rung->getBlock() != LoopHeader; Rung = Rung = Rung->getIDom())
  //   LatchPath.push_back(Rung->getBlock());
  // LatchPath.push_back(LoopHeader);

  // // Walk the instruction stream from the loop header to the loop latch.
  // for (BasicBlock *BB : reverse(LatchPath)) {
  //   for (Instruction &I : *BB) {
  //     // Skip instructions that weren't seen by IVUsers analysis.
  //     if (isa<PHINode>(I) || !IU.isIVUserOrOperand(&I))
  //       continue;

  //     // Ignore users that are part of a SCEV expression. (?)
  //     // ...?
  //     // I.dump();
  //     // if (SE.isSCEVable(I.getType()))
  //     //   SE.getSCEV(&I)->dump();
  //   }
  // }

  auto *LIS = &getAnalysis<LiveIntervals>();
  LIS->dump();
  
  return false;
}

char GASSIVDebug::ID = 0;
INITIALIZE_PASS_BEGIN(GASSIVDebug, DEBUG_TYPE, "GASS IV Debug", false, false)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(ScalarEvolutionWrapperPass)
INITIALIZE_PASS_DEPENDENCY(IVUsersWrapperPass)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_END(GASSIVDebug, DEBUG_TYPE, "GASS IV Debug", false, false)

Pass *llvm::createGASSIVDebugPass() { return new GASSIVDebug(); }