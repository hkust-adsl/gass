#ifndef LLVM_LIB_TARGET_GASS_GASSISELLOWERING_H
#define LLVM_LIB_TARGET_GASS_GASSISELLOWERING_H

// #include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {
class GASSSubtarget;

namespace GASSISD {
enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,

  EXIT,
};
} // namespace GASSISD

class GASSTargetLowering : public TargetLowering {
  const GASSSubtarget &Subtarget;

public:
  explicit GASSTargetLowering(const TargetMachine &TM,
                              const GASSSubtarget &STI);
  
  //=------DAG Combine--------------------------=//
  SDValue PerformDAGCombine(SDNode *N, DAGCombinerInfo &DCI) const override;

  //=---LowerCall, LowerFormalArguments, LowerReturn-----=//
  // Lower `call xxx`
  SDValue LowerCall(CallLoweringInfo &CLI, 
                    SmallVectorImpl<SDValue> &InVals) const override;
  // Lower foo(`args...`)
  // Called by LowerArguments().
  // Return new root of DAG (Chain)
  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool isVarArg, 
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &dl, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;
  // Lower `ret xxx`
  // Called by visitRet()
  // Return new root of DAG
  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &dl,
                      SelectionDAG &DAG) const override;
};

} // namespace llvm

#endif