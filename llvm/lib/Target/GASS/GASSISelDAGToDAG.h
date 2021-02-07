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
#include "GASSGenDAGISel.inc"
  void Select(SDNode *Node) override;
  bool tryLoad(SDNode *N);
  bool tryStore(SDNode *N);
  bool tryLDC(SDNode *N);

private:
  // other helper functions 
  bool selectDirectAddr(SDValue Value, SDValue &Addr);
  bool selectADDRri(SDValue Value, SDValue &Base, SDValue &Offset);
};
} // namespace llvm

#endif