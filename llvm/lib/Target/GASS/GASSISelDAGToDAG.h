#ifndef LLVM_LIB_TARGET_GASS_GASSISELDAGTODAG_H
#define LLVM_LIB_TARGET_GASS_GASSISELDAGTODAG_H

#include "GASSSubtarget.h"
#include "GASSTargetMachine.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/SelectionDAGISel.h"

namespace llvm {
class GASSSubtarget;

class GASSDAGToDAGISel : public SelectionDAGISel {
  // Cache Subtarget
  const GASSSubtarget *Subtarget;
public:
  explicit GASSDAGToDAGISel(GASSTargetMachine &TM)
    : SelectionDAGISel(TM) {}

  StringRef getPassName() const override {
    return "GASS DAG->DAG Pattern Instruction Selection";
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
#include "GASSGenDAGISel.inc"
  void Select(SDNode *Node) override;
  bool tryLoad(SDNode *N);
  bool tryStore(SDNode *N);
  bool tryLDC(SDNode *N);
  bool tryEXTRACT_VECTOR_ELT(SDNode *N);
  bool tryEXTRACT_SUBVECTOR(SDNode *N);
  bool tryBUILD_VECTOR(SDNode *N);
  bool trySHFL(SDNode *N);

private:
  // other helper functions 
  bool selectDirectAddr(SDValue Value, SDValue &Addr);
  bool selectADDRri(SDValue Value, SDValue &Base, SDValue &Offset);
  void selectBuildVector(SDNode *N, unsigned RegClassID);
};
} // namespace llvm

#endif