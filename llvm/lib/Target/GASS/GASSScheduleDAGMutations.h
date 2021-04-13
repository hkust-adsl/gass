#ifndef LLVM_LIB_TARGET_GASS_GASSSCHEDULEDAGMUTATIONS_H
#define LLVM_LIB_TARGET_GASS_GASSSCHEDULEDAGMUTATIONS_H

#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/ScheduleDAGMutation.h"
#include <memory>

namespace llvm {

std::unique_ptr<ScheduleDAGMutation> 
createGASSTensorCoreChainDAGMutation();

// Cluster IADDX to reduce predicate register pressure
std::unique_ptr<ScheduleDAGMutation>
createGASSCarryInClusterDAGMutation();

} // namespace llvm

#endif