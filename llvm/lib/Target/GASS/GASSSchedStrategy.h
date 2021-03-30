// Ref: AMDGPU/GCNSchedStrategy.h

#ifndef LLVM_LIB_TARGET_GASS_GASSSCHEDSTRATEGY_H
#define LLVM_LIB_TARGET_GASS_GASSSCHEDSTRATEGY_H

#include "llvm/CodeGen/MachineScheduler.h"

namespace llvm {
class GASSSchedStrategy final : public GenericScheduler {
public:
  SUnit *pickNode(bool &IsTopNode) override;

  // TODO: check them
  
};
} // namespace llvm

#endif