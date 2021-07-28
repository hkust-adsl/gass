#ifndef LLVM_LIB_TARGET_GASS_GASSISELLOWERING_H
#define LLVM_LIB_TARGET_GASS_GASSISELLOWERING_H

// #include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

namespace llvm {
class GASSSubtarget;

namespace GASSISD {
enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,
  Wrapper,

  CONSTANT_MEM,
  EXIT,

  LD,
  LDC,
  LDG,
  LDS,
  LDL,

  MOV,

  SETCC_LOGIC,
};
} // namespace GASSISD

class GASSTargetLowering : public TargetLowering {
  const GASSSubtarget &Subtarget;

public:
  explicit GASSTargetLowering(const TargetMachine &TM,
                              const GASSSubtarget &STI);
  
  const char *getTargetNodeName(unsigned Opcode) const override;

  // For setOperationAction(..., MVT::xx, Custom)
  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;
  
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

  // GASS always uses 32-bit shift amounts
  MVT getScalarShiftAmountTy(const DataLayout &, EVT) const override {
    return MVT::i32;
  }

  // override the default function to support UR
  const TargetRegisterClass* getRegClassFor(MVT VT, bool isDivergent) const override;

private:
  // Custom combining
  SDValue performLogicCombine(SDNode *N, DAGCombinerInfo &DCI) const;

private:
  // Custom lowering
  SDValue lowerBUILD_VECTOR(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerAddrSpaceCast(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerADD64(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerCONCAT_VECTORS(SDValue Op, SelectionDAG &DAG) const;
  SDValue lowerGlobalAddress(SDValue Op, SelectionDAG &DAG) const;

private:
  // utility functions
  SDValue JoinIntegers(SDValue Lo, SDValue Hi, SelectionDAG &DAG) const;
  void SplitInteger(SDValue Op, EVT LoVT, EVT HiVT, SDValue &Lo, SDValue &Hi,
                    SelectionDAG &DAG) const;
  void SplitInteger(SDValue Op, SDValue &Lo, SDValue &Hi, 
                    SelectionDAG &DAG) const;
  SDValue getAddr(GlobalAddressSDNode *N, SelectionDAG &DAG) const;
};

} // namespace llvm

#endif