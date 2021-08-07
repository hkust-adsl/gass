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

class SM80DepRemoveDAGMutation : public ScheduleDAGMutation {
public:
  SM80DepRemoveDAGMutation() {}
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

void SM80DepRemoveDAGMutation::apply(ScheduleDAGInstrs *DAG) {
    // remove deps ldsm -> ldgdepbar
  for (SUnit &SU : DAG->SUnits) {
    if (SU.getInstr()->getOpcode() == GASS::LDSM_x4_ri ||
        SU.getInstr()->getOpcode() == GASS::LDSM_x4_rui) {
      for (SDep &Pred : SU.Preds) {
        if (Pred.getSUnit()->getInstr()->getOpcode() == GASS::LDGDEPBAR) {
          // outs() << "Catch!\n";
          SUnit *PredSU = Pred.getSUnit();
          SU.removePred(Pred);
        }
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

std::unique_ptr<ScheduleDAGMutation> createGASSSM80DepRemoveDAGMutation() {
  return std::make_unique<SM80DepRemoveDAGMutation>();
}
} // namespace llvm