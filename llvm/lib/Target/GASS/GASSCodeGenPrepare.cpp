#include "GASS.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Pass.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/Transforms/Utils/IntegerDivision.h"

#define DEBUG_TYPE "gass-codgenprepare"

using namespace llvm;

namespace {
class GASSCodeGenPrepare : public FunctionPass {
  bool visit(Instruction &I);
public:
  static char ID;

  GASSCodeGenPrepare() : FunctionPass(ID) {}

  StringRef getPassName() const override { return "GASS IR Optimizations"; }

  bool runOnFunction(Function &F) override;
};
} // anonymous namespace


bool GASSCodeGenPrepare::visit(Instruction &I) {
  switch (I.getOpcode()) {
  default: return false;
  case Instruction::URem: case Instruction::SRem: {
    BinaryOperator *BO = dyn_cast<BinaryOperator>(&I);
    assert(BO != nullptr);
    Value *Den = I.getOperand(1);
    if (isa<Constant>(Den)) return false;
    expandRemainder(BO);
    return true;
  }
  case Instruction::UDiv: case Instruction::SDiv: {
    BinaryOperator *BO = dyn_cast<BinaryOperator>(&I);
    assert(BO != nullptr);
    Value *Den = I.getOperand(1);
    if (isa<Constant>(Den)) return false;
    expandDivision(BO);
    return true;
  }
  }
}

bool GASSCodeGenPrepare::runOnFunction(Function &F) {
  bool MadeChange = false;

  Function::iterator NextBB;
  for (Function::iterator FI = F.begin(), FE = F.end(); FI != FE; FI = NextBB) {
    BasicBlock *BB = &*FI;
    NextBB = std::next(FI);

    BasicBlock::iterator Next;
    for (BasicBlock::iterator I = BB->begin(), E = BB->end(); 
         I != E; I = Next) {
      Next = std::next(I);

      MadeChange |= visit(*I);

      if (Next != E) { // Control flow changed
        BasicBlock *NextInstBB = Next->getParent();
        if (NextInstBB != BB) {
          BB = NextInstBB;
          E = BB->end();
          FE = F.end();
        }
      }
    }
  }

  F.dump();

  return MadeChange;
}

INITIALIZE_PASS(GASSCodeGenPrepare, DEBUG_TYPE,
                "GASS IR Optimizations", false, false)

char GASSCodeGenPrepare::ID = 0;

FunctionPass *llvm::createGASSCodeGenPreparePass() {
  return new GASSCodeGenPrepare();
}