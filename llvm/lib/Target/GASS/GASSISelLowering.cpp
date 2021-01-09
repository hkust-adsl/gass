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
  
  // ConstantFP are legal in GASS (*Must*, otherwise will be expanded to 
  //                                                   load<ConstantPool>)
  setOperationAction(ISD::ConstantFP, MVT::f64, Legal);
  setOperationAction(ISD::ConstantFP, MVT::f32, Legal);
  setOperationAction(ISD::ConstantFP, MVT::f16, Legal);

  computeRegisterProperties(Subtarget.getRegisterInfo());
}

const char *GASSTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  case GASSISD::EXIT: return "GASSISD::EXIT";
  case GASSISD::MOV:  return "GASSISD::MOV";
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
    SDValue ParamNode = DAG.getNode(GASSISD::MOV, dl, ObjectVT, 
                                    DAG.getConstant(CBankOff, dl, MVT::i32));
    InVals.push_back(ParamNode);

    // Move forward CBankOff
    CBankOff += 4;
  }

  return Chain;
}