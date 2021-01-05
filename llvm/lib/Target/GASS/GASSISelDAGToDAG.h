#ifndef LLVM_LIB_TARGET_GASS_GASSISELDAGTODAG_H
#define LLVM_LIB_TARGET_GASS_GASSISELDAGTODAG_H

#include "GASSTargetMachine.h"
#include "llvm/CodeGen/SelectionDAGISel.h"

namespace llvm {
class GASSDAGToDAGISel : public SelectionDAGISel {
public:
  explicit GASSDAGToDAGISel(GASSTargetMachine &TM)
    : SelectionDAGISel(TM) {}

  StringRef getPassName() const override {
    return "GASS DAG->DAG Pattern Instruction Selection";
  }

private:
  void Select(SDNode *Node) override;
#include "GASSGenDAGISel.inc"
};
} // namespace llvm

#endif