#ifndef LLVM_LIB_TARGET_GASS_GASSTARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_GASS_GASSTARGETTRANSFORMINFO_H

#include "GASS.h"
#include "GASSTargetMachine.h"
#include "GASSSubtarget.h"
#include "GASSISelLowering.h"
#include "llvm/CodeGen/BasicTTIImpl.h"

namespace llvm {

class GASSTTIImpl final : public BasicTTIImplBase<GASSTTIImpl> {
  typedef BasicTTIImplBase<GASSTTIImpl> BaseT;
  const GASSSubtarget *STI;
  const GASSTargetLowering *TLI;
public:
  explicit GASSTTIImpl(const GASSTargetMachine *TM, const Function &F)
    : BaseT(TM, F.getParent()->getDataLayout()), STI(TM->getSubtargetImpl()), 
      TLI(STI->getTargetLowering()) {}

  // TODO: What's this for?
  unsigned getFlatAddressSpace() const {
    // return static_cast<unsigned>(AddressSpace::GENERIC);
    return 0;
  }

  // *Must* be implemented 
  const GASSTargetLowering *getTLI() const { return TLI; }

  // Required by DivergenceAnalysis
  bool hasBranchDivergence() { return true; }
  bool isSourceOfDivergence(const Value *V);
}; // GASSTTIImpl

} // end namespace llvm

#endif