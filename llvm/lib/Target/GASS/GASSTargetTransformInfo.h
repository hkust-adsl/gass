#ifndef LLVM_LIB_TARGET_GASS_GASSTARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_GASS_GASSTARGETTRANSFORMINFO_H

#include "GASS.h"
#include "GASSTargetMachine.h"
#include "GASSSubtarget.h"
#include "GASSISelLowering.h"
#include "llvm/CodeGen/BasicTTIImpl.h"

namespace llvm {

// TODO: Why not this overrides any function?
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
    return static_cast<unsigned>(AddressSpace::GENERIC);
  }

  // *Must* be implemented 
  const GASSTargetLowering *getTLI() const { return TLI; }

  
}; // GASSTTIImpl
}

#endif