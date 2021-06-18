//===----------------------------------------------------------------------===//
//
/// \file
/// This is mostly a copy-and-paste from AMDGPUAnnotateUniformValues.cpp
//
//===----------------------------------------------------------------------===//

#include "GASS.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"
#include "llvm/Pass.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Analysis/LegacyDivergenceAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/InitializePasses.h"

#define DEBUG_TYPE "gass-annotate-uniform"

using namespace llvm;

namespace {
class GASSAnnotateUniformValues : public FunctionPass {
  LegacyDivergenceAnalysis *DA;
  MemoryDependenceResults *MDR;
  LoopInfo *LI;
public:
  static char ID;
  GASSAnnotateUniformValues() : FunctionPass(ID) {}

  bool runOnFunction(Function &F) override;
  StringRef getPassName() const override { 
    return "GASS Annotate Uniform Values";
  }
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LegacyDivergenceAnalysis>();
    AU.addRequired<MemoryDependenceWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.setPreservesAll();
  }
private:
  bool visitInstr(Instruction *I);
};
} // anonymous namespace

INITIALIZE_PASS_BEGIN(GASSAnnotateUniformValues, DEBUG_TYPE, 
                      "GASS Annotate Uniform Values", false, false)
INITIALIZE_PASS_DEPENDENCY(LegacyDivergenceAnalysis)
INITIALIZE_PASS_DEPENDENCY(MemoryDependenceWrapperPass)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_END(GASSAnnotateUniformValues, DEBUG_TYPE,
                    "GASS Annotate Uniform Values", false, false)

char GASSAnnotateUniformValues::ID = 0;

static void setUniformMetadata(Instruction *I) {
  I->setMetadata("gass.uniform", MDNode::get(I->getContext(), {}));
}

// Returns true if made change
bool GASSAnnotateUniformValues::visitInstr(Instruction *I) {
  if (DA->isUniform(I)) {
    setUniformMetadata(I);
    I->dump();
    return true;
  }
  return false;
}

bool GASSAnnotateUniformValues::runOnFunction(Function &F) {
  DA = &getAnalysis<LegacyDivergenceAnalysis>();
  MDR = &getAnalysis<MemoryDependenceWrapperPass>().getMemDep();
  LI = &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

  bool MadeChange = false;

  for (BasicBlock &BB : F)
    for (Instruction &I : BB)
      MadeChange |= visitInstr(&I);

  return MadeChange;
}

FunctionPass*
llvm::createGASSAnnotateUniformValues() {
  return new GASSAnnotateUniformValues();
}