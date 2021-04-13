#include "MCTargetDesc/GASSMCTargetDesc.h"
#include "GASSScheduleDAGMutations.h"
#include "llvm/CodeGen/ScheduleDAG.h"
#include "llvm/CodeGen/ScheduleDAGMutation.h"
#include <memory>

//===----------------------------------------------------------------------===//
// This file contains a DAG scheduling mutation to add chain to all 
// Tensor Core instructions.
//===----------------------------------------------------------------------===//

using namespace llvm;

namespace {
class TensorCoreChainDAGMutation : public ScheduleDAGMutation {
public:
  TensorCoreChainDAGMutation() {}
  void apply(ScheduleDAGInstrs *DAG) override;
};
} // anonymous namespace

// Let Tensor Core instructions 
void TensorCoreChainDAGMutation::apply(ScheduleDAGInstrs *DAG) {
  // pseudo code
  // for (SU : SUs) {
  //   if (isTC()) {
  //     SU->addPred(pred) // DAG->addEdge()?
  //   }
  // }
  SUnit *PrevTC = nullptr;
  for(auto iter = DAG->begin(); iter != DAG->end(); ++iter) {
    // TODO: should query target (e.g., TII->isTensorCoreInstr(*iter))
    if (iter->getOpcode() == GASS::HMMA884_f32_f32) {
      if (PrevTC == nullptr)
        PrevTC = DAG->getSUnit(&*iter);
      else {
        SUnit *CurrTC = DAG->getSUnit(&*iter);
        assert(DAG->canAddEdge(CurrTC, PrevTC));
        DAG->addEdge(CurrTC, SDep(PrevTC, SDep::Artificial));
        PrevTC = CurrTC;
      }
    }
  }
}

//==------------------------------------------------------------------------==//
// public interface
//==------------------------------------------------------------------------==//
namespace llvm {
std::unique_ptr<ScheduleDAGMutation> createGASSTensorCoreChainDAGMutation() {
  return std::make_unique<TensorCoreChainDAGMutation>();
}


} // namespace llvm