#include "GASSISelLowering.h"
#include "GASSSubtarget.h"
#include "GASSTargetMachine.h"

using namespace llvm;

#define DEBUG_TYPE "gass-lower"

GASSTargetLowering::GASSTargetLowering(const TargetMachine &TM,
                                       const GASSSubtarget &STI)
  : TargetLowering(TM), Subtarget(STI) {
  addRegisterClass(MVT::i32, &GASS::VReg32RegClass);
  addRegisterClass(MVT::f32, &GASS::VReg32RegClass);
  addRegisterClass(MVT::i64, &GASS::VReg64RegClass);
  addRegisterClass(MVT::f64, &GASS::VReg64RegClass);
  
  // ConstantFP are legal in GASS (*Must*, otherwise will be expanded to 
  //                                                   load<ConstantPool>)
  setOperationAction(ISD::ConstantFP, MVT::f64, Legal);
  setOperationAction(ISD::ConstantFP, MVT::f32, Legal);
  setOperationAction(ISD::ConstantFP, MVT::f16, Legal);

  // Custom
  setOperationAction(ISD::ADDRSPACECAST, MVT::i32, Custom);
  setOperationAction(ISD::ADDRSPACECAST, MVT::i64, Custom);

  computeRegisterProperties(Subtarget.getRegisterInfo());
}

const char *GASSTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  default: return "Unknown Node Name";
  case GASSISD::EXIT: return "GASSISD::EXIT";
  case GASSISD::MOV:  return "GASSISD::MOV";
  case GASSISD::LDC:  return "GASSISD::LDC";
  }
}

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
  return DAG.getNode(GASSISD::EXIT, dl, MVT::Other, Chain);
}

// Ins: ISD::InputArg. Mainly about type (VT)
// This function is used to construct InVals
SDValue GASSTargetLowering::LowerFormalArguments(
  SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
  const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &dl,
  SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  // NVGPUs have unique way to pass kernel parameters, we can't
  // leverage callingconv.
  MachineFunction &MF = DAG.getMachineFunction();
  const DataLayout &DL = DAG.getDataLayout();

  const Function *F = &MF.getFunction();

  unsigned CBankOff = Subtarget.getParamBase();

  // Cache ArgInfo
  std::vector<Type *> ArgsType;
  std::vector<const Argument*> TheArgs;
  for (const Argument &I : F->args()) {
    ArgsType.push_back(I.getType());
    TheArgs.push_back(&I);
  }

  // FIXME (dyan): To support different types.
  unsigned NumArgs = Ins.size();
  for (unsigned i = 0; i != TheArgs.size(); ++i) {
    // LDC.$Width or MOV?
    Type *Ty = ArgsType[i];
    EVT ObjectVT = getValueType(DL, Ty);
    SDValue ParamNode = DAG.getNode(GASSISD::LDC, dl, ObjectVT, 
                                    DAG.getConstant(CBankOff, dl, ObjectVT)); 
    InVals.push_back(ParamNode);

    // Move forward CBankOff
    CBankOff += 4;
  }

  return Chain;
}

SDValue 
GASSTargetLowering::LowerOperation(SDValue Op, SelectionDAG &DAG) const {
  switch(Op.getOpcode()) {
  default:
    llvm_unreachable("Custom lowering not defined for operation");
  case ISD::ADDRSPACECAST: return lowerAddrSpaceCast(Op, DAG);
  }
}

//=--------------------------------------=//
// private lowering
//=--------------------------------------=//
SDValue
GASSTargetLowering::lowerAddrSpaceCast(SDValue Op, SelectionDAG &DAG) const {
  SDLoc dl(Op);
  SDValue Src = Op.getOperand(0);
  MVT DstVT = Op.getSimpleValueType(); // Which is?

  // Always truncate
  Op = DAG.getNode(ISD::TRUNCATE, dl, DstVT, Src);

  return Op;
}