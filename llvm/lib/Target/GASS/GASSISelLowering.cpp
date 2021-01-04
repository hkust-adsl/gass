#include "GASSISelLowering.h"

using namespace llvm;

#define DEBUG_TYPE "gass-lower"

GASSTargetLowering::GASSTargetLowering(const TargetMachine &TM,
                                       const GASSSubtarget &STI)
  : TargetLowering(TM), Subtarget(STI) {}

//=------------------------------------------=//
// DAG combine
//=------------------------------------------=//
SDValue GASSTargetLowering::PerformDAGCombine(SDNode *N, 
                                              DAGCombinerInfo &DCI) const {
  // TODO: fill this.
  return SDValue();
}

//=---------------------------------------------------------------=//
// Function Lowering (LowerCall, LowerReturn, LowerFormalArguments)
//=---------------------------------------------------------------=//
SDValue
GASSTargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                              SmallVectorImpl<SDValue> &InVals) const {
  // TODO: fill this.
  return SDValue();
}

SDValue 
GASSTargetLowering::LowerReturn(SDValue Chain, CallingConv::ID CallConv,
                                bool isVarArg,
                                const SmallVectorImpl<ISD::OutputArg> &Outs,
                                const SmallVectorImpl<SDValue> &OutVals,
                                const SDLoc &dl, SelectionDAG &DAG) const {
  // TODO: fill this.
  return Chain;
}

// Ins: ISD::InputArg. Mainly about type (VT)
// This function is used to construct InVals
// Returns NewRoot
SDValue GASSTargetLowering::LowerFormalArguments(
  SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
  const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &dl,
  SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  // TODO: fill this. 
  
  return Chain;
}