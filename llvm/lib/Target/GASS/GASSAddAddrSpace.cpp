#include "GASS.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"

using namespace llvm;

// This is weird.
namespace llvm {
void initializeGASSAddAddrSpacePass(PassRegistry &);
}

namespace {
class GASSAddAddrSpace : public FunctionPass {
  bool runOnFunction(Function &F) override;

  void markPointerAsGlobal(Value *Ptr);

public:
  static char ID;

  GASSAddAddrSpace(const GASSTargetMachine *TM = nullptr)
    : FunctionPass(ID) {}

  StringRef getPassName() const override {
    return "Add addrspacecast for CUDA kernels";
  }
};
} // namespace

char GASSAddAddrSpace::ID = 1;

INITIALIZE_PASS(GASSAddAddrSpace, "gass-add-addrspace",
                "Add addrspacecast (GASS)", false, false)

bool GASSAddAddrSpace::runOnFunction(Function &F) {
  for (Argument &Arg : F.args())
    if (Arg.getType()->isPointerTy())
      markPointerAsGlobal(&Arg);

  return true;
}

// This function adds addrspacecast for pointer parameters
void GASSAddAddrSpace::markPointerAsGlobal(Value *Ptr) {
  if (Ptr->getType()->getPointerAddressSpace() == GASS::GLOBAL)
    return;

  BasicBlock::iterator InsertPt = 
      dyn_cast<Argument>(Ptr)->getParent()->getEntryBlock().begin();
  
  // insert 2 addrspacecast for each ptr param
  Instruction *PtrInGlobal = new AddrSpaceCastInst(
    /*Value*/Ptr, 
    /*Type *Ty*/PointerType::get(Ptr->getType()->getPointerElementType(), GASS::GLOBAL),
    /*Twine Name*/Ptr->getName(), 
    /*Instruction* InsertBefore*/&*InsertPt);
  Value *PtrInGeneric = new AddrSpaceCastInst(PtrInGlobal, Ptr->getType(), 
                                              Ptr->getName(), &*InsertPt);

  // TODO: why we need this?
  Ptr->replaceAllUsesWith(PtrInGeneric);
  PtrInGlobal->setOperand(0, Ptr);
}

// create Pass
FunctionPass *
llvm::createGASSAddrSpacePass() {
  return new GASSAddAddrSpace();
}