#include "GASSISelLowering.h"
#include "GASS.h"
#include "GASSSubtarget.h"
#include "GASSTargetMachine.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/TableGen/Record.h"

using namespace llvm;

#define DEBUG_TYPE "gass-lower"

GASSTargetLowering::GASSTargetLowering(const TargetMachine &TM,
                                       const GASSSubtarget &STI)
  : TargetLowering(TM), Subtarget(STI) {
  addRegisterClass(MVT::i1,  &GASS::VReg1RegClass);
  addRegisterClass(MVT::i32, &GASS::VReg32RegClass);
  addRegisterClass(MVT::f32, &GASS::VReg32RegClass);

  addRegisterClass(MVT::i64, &GASS::VReg64RegClass);
  addRegisterClass(MVT::f64, &GASS::VReg64RegClass);

  addRegisterClass(MVT::i128, &GASS::VReg128RegClass);
  // Vector types
  addRegisterClass(MVT::v2f16, &GASS::VReg32RegClass);
  addRegisterClass(MVT::v4f16, &GASS::VReg64RegClass);
  addRegisterClass(MVT::v8f16, &GASS::VReg128RegClass);
  addRegisterClass(MVT::v2f32, &GASS::VReg64RegClass);
  addRegisterClass(MVT::v4f32, &GASS::VReg128RegClass);
  addRegisterClass(MVT::v2i32, &GASS::VReg64RegClass);
  addRegisterClass(MVT::v4i32, &GASS::VReg128RegClass);
  // sub-register
  // AMGDPU does this anyway
  addRegisterClass(MVT::f16, &GASS::VReg32RegClass);
  
  // ConstantFP are legal in GASS (*Must*, otherwise will be expanded to 
  //                                                   load<ConstantPool>)
  setOperationAction(ISD::ConstantFP, MVT::f64, Legal);
  setOperationAction(ISD::ConstantFP, MVT::f32, Legal);
  setOperationAction(ISD::ConstantFP, MVT::f16, Legal);
  setOperationAction(ISD::Constant, MVT::i32, Legal);
  setOperationAction(ISD::Constant, MVT::i64, Legal);

  // expand i64 instructions: ADD, SHF & ?
  // i64 add -> 2x i32 add (or use psedo instruction?)
  // setOperationAction(ISD::ADD, MVT::i64, Custom);

  // TODO: What does this mean?
  // Operations not directly supported by GASS. (NVPTX)
  for (MVT VT : {MVT::f16, MVT::v2f16, MVT::f32, MVT::f64, MVT::i1, MVT::i8,
                 MVT::i16, MVT::i32, MVT::i64}) {
    setOperationAction(ISD::SELECT_CC, VT, Expand);
    setOperationAction(ISD::BR_CC, VT, Expand);
  }

  // These map to corresponding instructions for f32/f64. f16 must be
  // promoted to f32. v2f16 is expanded to f16, which is then promoted
  // to f32.
  for (const auto &Op : {ISD::FDIV, ISD::FREM, ISD::FSQRT, ISD::FSIN, ISD::FCOS,
                         ISD::FABS, ISD::FMINNUM, ISD::FMAXNUM}) {
    setOperationAction(Op, MVT::f16, Promote);
    setOperationAction(Op, MVT::f32, Legal);
    setOperationAction(Op, MVT::f64, Legal);
    setOperationAction(Op, MVT::v2f16, Expand);
  }

  // Provide all sorts of operation actions
  setOperationAction(ISD::ADDCARRY, MVT::i32, Legal);

  // Remove addrspacecast instr
  setOperationAction(ISD::ADDRSPACECAST, MVT::i32, Custom);
  setOperationAction(ISD::ADDRSPACECAST, MVT::i64, Custom);

  setOperationAction(ISD::GlobalAddress, MVT::i64, Custom);
  setOperationAction(ISD::GlobalAddress, MVT::i32, Custom);

  // Vector operations (required)
  // setOperationAction(ISD::BUILD_VECTOR, MVT::v4f32, Expand);
  // setOperationAction(ISD::EXTRACT_VECTOR_ELT, MVT::v4f32, Legal);
  // setOperationAction(ISD::INSERT_VECTOR_ELT, MVT::v4f32, Legal);
  // setOperationAction(ISD::VECTOR_SHUFFLE, MVT::v4f32, Legal);
  // setOperationAction(ISD::CONCAT_VECTORS, MVT::v4f32, Legal);

  // TODO: logic here can also be applied to v2i16, v4i16 ... v2i8 ...
  setOperationAction(ISD::BUILD_VECTOR, MVT::v2f16, Custom);
  setOperationAction(ISD::BUILD_VECTOR, MVT::v4f16, Custom);
  setOperationAction(ISD::BUILD_VECTOR, MVT::v8f16, Custom);

  // setOperationAction(ISD::EXTRACT_VECTOR_ELT, MVT::v4f16, Expand);
  // setOperationAction(ISD::EXTRACT_VECTOR_ELT, MVT::v8f16, Expand);

  // Turn FP truncstore into trunc + store.
  // FIXME: vector types should also be expanded
  setTruncStoreAction(MVT::f32, MVT::f16, Expand);
  setTruncStoreAction(MVT::f64, MVT::f16, Expand);
  setTruncStoreAction(MVT::f64, MVT::f32, Expand);

  // Custom DAG combine patterns
  setTargetDAGCombine(ISD::AND);
  setTargetDAGCombine(ISD::XOR);
  setTargetDAGCombine(ISD::OR);

  computeRegisterProperties(Subtarget.getRegisterInfo());
}

const char *GASSTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  default: return "Unknown Node Name";
  case GASSISD::Wrapper: return "GASSISD::Wrapper";
  case GASSISD::EXIT: return "GASSISD::EXIT";
  case GASSISD::MOV:  return "GASSISD::MOV";
  case GASSISD::LDC:  return "GASSISD::LDC";
  case GASSISD::SETCC_LOGIC: return "GASSISD::SETCC_LOGIC";
  }
}

//=------------------------------------------=//
// DAG combine
//=------------------------------------------=//
/// This method will be invoked for all target nodes and for any
/// target-independent nodes that the target has registered with invoke it
/// for.
///
/// The semantics are as follows:
/// Return Value:
///   SDValue.Val == 0   - No change was made
///   SDValue.Val == N   - N was replaced, is dead, and is already handled.
///   otherwise          - N should be replaced by the returned Operand.
///
/// In addition, methods provided by DAGCombinerInfo may be used to perform
/// more complex transformations.
///
SDValue GASSTargetLowering::PerformDAGCombine(SDNode *N, 
                                              DAGCombinerInfo &DCI) const {
  switch (N->getOpcode()) {
  default: break;
  case ISD::XOR: case ISD::AND: case ISD::OR:
    return performLogicCombine(N, DCI);
  }
  return SDValue();
}


static std::pair<GASS::GASSCC::CondCode, GASS::GASSCC::CondCodeSign> 
getGASSCC(ISD::CondCode CC) {
  switch (CC) {
  default: llvm_unreachable("Error");
  case ISD::SETEQ: return std::make_pair(GASS::GASSCC::EQ, GASS::GASSCC::S32);
  case ISD::SETNE: return std::make_pair(GASS::GASSCC::NE, GASS::GASSCC::S32);
  case ISD::SETLT: return std::make_pair(GASS::GASSCC::LT, GASS::GASSCC::S32);
  case ISD::SETLE: return std::make_pair(GASS::GASSCC::LE, GASS::GASSCC::S32);
  case ISD::SETGT: return std::make_pair(GASS::GASSCC::GT, GASS::GASSCC::S32);
  case ISD::SETGE: return std::make_pair(GASS::GASSCC::GE, GASS::GASSCC::S32);
  case ISD::SETUEQ: return std::make_pair(GASS::GASSCC::EQU, GASS::GASSCC::U32);
  case ISD::SETUNE: return std::make_pair(GASS::GASSCC::NEU, GASS::GASSCC::U32);
  case ISD::SETULT: return std::make_pair(GASS::GASSCC::LTU, GASS::GASSCC::U32);
  case ISD::SETULE: return std::make_pair(GASS::GASSCC::LEU, GASS::GASSCC::U32);
  case ISD::SETUGT: return std::make_pair(GASS::GASSCC::GTU, GASS::GASSCC::U32);
  case ISD::SETUGE: return std::make_pair(GASS::GASSCC::GEU, GASS::GASSCC::U32);
  }
}
/// (and (setp a, b), lop) -> (setp.and a, b, lop)
/// (xor (setp a, b), lop) -> (setp.xor a, b, lop)
/// (or  (setp a, b), lop) -> (setp.or  a, b, lop)
SDValue GASSTargetLowering::performLogicCombine(SDNode *N, 
                                                DAGCombinerInfo &DCI) const {
  SDLoc DL(N);
  SDValue Op0 = N->getOperand(0);
  SDValue Op1 = N->getOperand(1);
  MVT VT = N->getSimpleValueType(0);
  // We only combine i1 bool op
  if (VT != MVT::i1)
    return SDValue();

  SelectionDAG &DAG = DCI.DAG;
  GASS::GASSCC::LogicOp LogicOp;
  switch (N->getOpcode()) {
  default: llvm_unreachable("Error");
  case ISD::AND: LogicOp = GASS::GASSCC::AND; break;
  case ISD::XOR: LogicOp = GASS::GASSCC::XOR; break;
  case ISD::OR:  LogicOp = GASS::GASSCC::OR;  break;
  }
  SDValue LogicOpVal = DAG.getConstant(LogicOp, DL, MVT::i32);

  GASS::GASSCC::CondCode GCC;
  GASS::GASSCC::CondCodeSign GCCSign;
  // try Op0
  if (Op0.getOpcode() == ISD::SETCC) {
    ISD::CondCode CC = cast<CondCodeSDNode>(Op0.getOperand(2))->get();
    std::tie(GCC, GCCSign) = getGASSCC(CC);
    SDValue GCCOp = DAG.getConstant(GCC, DL, MVT::i32);
    SDValue GCCSignOp = DAG.getConstant(GCCSign, DL, MVT::i32);
    // Combine success. Do the folding.
    SDValue Ops[] = {Op0.getOperand(0), Op0.getOperand(1), Op1, 
                     GCCOp, GCCSignOp, LogicOpVal};
    return DAG.getNode(GASSISD::SETCC_LOGIC, DL, VT, Ops);
  }

  // try Op1
  if (Op1.getOpcode() == ISD::SETCC) {
    ISD::CondCode CC = cast<CondCodeSDNode>(Op0.getOperand(2))->get();
    std::tie(GCC, GCCSign) = getGASSCC(CC);
    // Combine Success
    SDValue GCCOp = DAG.getConstant(GCC, DL, MVT::i32);
    SDValue GCCSignOp = DAG.getConstant(GCCSign, DL, MVT::i32);
    SDValue Ops[] = {Op1.getOperand(0), Op1.getOperand(1), Op0, 
                     GCCOp, GCCSignOp, LogicOpVal};
    return DAG.getNode(GASSISD::SETCC_LOGIC, DL, VT, Ops);
  }

  /// Combine failed
  return SDValue();
}

//=---------------------------------------------------------------=//
// Function Lowering (LowerCall, LowerReturn, LowerFormalArguments)
//=---------------------------------------------------------------=//
SDValue
GASSTargetLowering::LowerCall(TargetLowering::CallLoweringInfo &CLI,
                              SmallVectorImpl<SDValue> &InVals) const {
  // TODO: fill this.  
  CLI.Callee->dump();
  CLI.DAG.dump();
  llvm_unreachable("GASSTargetLowering::LowerCall() not implemented");
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

static unsigned alignTo(unsigned CurOff, unsigned AlignReq) {
  while (CurOff % AlignReq != 0)
    CurOff++;

  return CurOff;    
}

// Ins: ISD::InputArg. Mainly about type (VT)
// This function is used to construct InVals
/// @param Chain Chain
/// @param CallConv
/// @param isVarArg F.isVarArg() if function takes variable number of args
/// @param Ins Flags
/// @param dl
/// @param DAG
/// @param InVals Argument values to be set up
SDValue GASSTargetLowering::LowerFormalArguments(
  SDValue Chain, CallingConv::ID CallConv, bool isVarArg,
  const SmallVectorImpl<ISD::InputArg> &Ins, const SDLoc &dl,
  SelectionDAG &DAG, SmallVectorImpl<SDValue> &InVals) const {
  // NVGPUs have unique way to pass kernel parameters, we can't
  // leverage callingconv.
  MachineFunction &MF = DAG.getMachineFunction();
  const DataLayout &DL = DAG.getDataLayout();

  const Function *F = &MF.getFunction();

  // Base constant bank offset. This value is target-dependent
  // For Volta/Turing, it's 0x160
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
    assert(ObjectVT.getScalarSizeInBits() % 8 == 0 &&
           "Sub-byte argument not supported yet, to be extended");
    unsigned SizeInBytes = ObjectVT.getScalarSizeInBits() / 8;
    unsigned AlignRequirement = DL.getABITypeAlignment(Ty);
    CBankOff = alignTo(CBankOff, AlignRequirement);
    SDValue ParamNode = DAG.getNode(GASSISD::LDC, dl, ObjectVT, 
                                    DAG.getConstant(CBankOff, dl, MVT::i32)); 
    InVals.push_back(ParamNode);

    // Move forward CBankOff
    CBankOff += SizeInBytes;
  }

  return Chain;
}

SDValue 
GASSTargetLowering::LowerOperation(SDValue Op, SelectionDAG &DAG) const {
  switch(Op.getOpcode()) {
  default:
    llvm_unreachable("Custom lowering not defined for operation");
  case ISD::BUILD_VECTOR: return lowerBUILD_VECTOR(Op, DAG);
  case ISD::ADDRSPACECAST: return lowerAddrSpaceCast(Op, DAG);
  case ISD::ADD: return lowerADD64(Op, DAG);
  case ISD::CONCAT_VECTORS: return lowerCONCAT_VECTORS(Op, DAG);
  case ISD::GlobalAddress: return lowerGlobalAddress(Op, DAG);
  }
}

//=--------------------------------------=//
// Custom lowering
//=--------------------------------------=//
SDValue 
GASSTargetLowering::lowerBUILD_VECTOR(SDValue Op, SelectionDAG &DAG) const {
  // BUILD_VECTOR
  // for v2f16, v4f16, v8f16
  // Res = BUILD_VECTOR ConstantFP:f16<APFloat(0)>, ....
  //   ===>
  // Res = BITCAST Constant;
  // 1. If all operands are ConstantFP
  auto isConstantFPLowering = [&Op]() -> bool {
    for (unsigned OpNo = 0; OpNo < Op.getNumOperands(); ++OpNo) {
      if (!isa<ConstantFPSDNode>(Op.getOperand(OpNo)))
        return false;
    }
    return true;
  };

  MVT VT = Op.getSimpleValueType();
  unsigned NumElts = VT.getVectorNumElements();
  MVT ScalarTy = VT.getScalarType();
  SDLoc DL(Op);

  // Focus on f16 now
  if (isConstantFPLowering() && ScalarTy == MVT::f16) {
    if (VT == MVT::v2f16) {
      APInt E0 =
        cast<ConstantFPSDNode>(Op->getOperand(0))->getValueAPF()
                                                   .bitcastToAPInt();
      APInt E1 =
        cast<ConstantFPSDNode>(Op->getOperand(1))->getValueAPF()
                                                   .bitcastToAPInt();
      SDValue Const =
          DAG.getConstant(E1.zext(32).shl(16) | E0.zext(32), 
                          SDLoc(Op), MVT::i32);
      return DAG.getNode(ISD::BITCAST, SDLoc(Op), MVT::v2f16, Const);
    } else if (VT == MVT::v4f16) {
      APInt E0 =
        cast<ConstantFPSDNode>(Op->getOperand(0))->getValueAPF()
                                                   .bitcastToAPInt();
      APInt E1 =
        cast<ConstantFPSDNode>(Op->getOperand(1))->getValueAPF()
                                                   .bitcastToAPInt();
      APInt E2 =
        cast<ConstantFPSDNode>(Op->getOperand(2))->getValueAPF()
                                                   .bitcastToAPInt();
      APInt E3 =
        cast<ConstantFPSDNode>(Op->getOperand(3))->getValueAPF()
                                                   .bitcastToAPInt();
      SDValue Const =
        DAG.getConstant(
            E3.zext(64).shl(48) | E2.zext(64).shl(32) | 
            E1.zext(64).shl(16) | E0.zext(64), SDLoc(Op), MVT::i64);
      return DAG.getNode(ISD::BITCAST, SDLoc(Op), MVT::v4f16, Const);
    } else if (VT == MVT::v8f16) {
      // split i128 result into 2 i64
      // TODO: update GASSISelLowering::tryBUILD_VECTOR to support 64-bit vector
      APInt E0 =
        cast<ConstantFPSDNode>(Op->getOperand(0))->getValueAPF()
                                                   .bitcastToAPInt();
      APInt E1 =
        cast<ConstantFPSDNode>(Op->getOperand(1))->getValueAPF()
                                                   .bitcastToAPInt();
      APInt E2 =
        cast<ConstantFPSDNode>(Op->getOperand(2))->getValueAPF()
                                                   .bitcastToAPInt();
      APInt E3 =
        cast<ConstantFPSDNode>(Op->getOperand(3))->getValueAPF()
                                                   .bitcastToAPInt();
      APInt E4 =
        cast<ConstantFPSDNode>(Op->getOperand(4))->getValueAPF()
                                                   .bitcastToAPInt();
      APInt E5 =
        cast<ConstantFPSDNode>(Op->getOperand(5))->getValueAPF()
                                                   .bitcastToAPInt();
      APInt E6 =
        cast<ConstantFPSDNode>(Op->getOperand(6))->getValueAPF()
                                                   .bitcastToAPInt();
      APInt E7 =
        cast<ConstantFPSDNode>(Op->getOperand(7))->getValueAPF()
                                                   .bitcastToAPInt();
      SDValue Const0 = DAG.getConstant(E1.zext(32).shl(16) | E0.zext(32), 
                                       DL, MVT::i32);
      SDValue Const1 = DAG.getConstant(E3.zext(32).shl(16) | E2.zext(32), 
                                       DL, MVT::i32);
      SDValue Const2 = DAG.getConstant(E5.zext(32).shl(16) | E4.zext(32), 
                                       DL, MVT::i32);
      SDValue Const3 = DAG.getConstant(E7.zext(32).shl(16) | E6.zext(32), 
                                       DL, MVT::i32);
      SDValue Res0 = DAG.getNode(ISD::BITCAST, DL, MVT::i32, Const0);
      SDValue Res1 = DAG.getNode(ISD::BITCAST, DL, MVT::i32, Const1);
      SDValue Res2 = DAG.getNode(ISD::BITCAST, DL, MVT::i32, Const2);
      SDValue Res3 = DAG.getNode(ISD::BITCAST, DL, MVT::i32, Const3);

      SDValue Ops[] = {DAG.getTargetConstant(GASS::VReg128RegClassID, DL, 
                                             MVT::i32),
                       Res0, DAG.getTargetConstant(GASS::sub0, DL, MVT::i32),
                       Res1, DAG.getTargetConstant(GASS::sub1, DL, MVT::i32),
                       Res2, DAG.getTargetConstant(GASS::sub2, DL, MVT::i32),
                       Res3, DAG.getTargetConstant(GASS::sub3, DL, MVT::i32)};
      return SDValue(
          DAG.getMachineNode(TargetOpcode::REG_SEQUENCE, DL, MVT::v8f16, Ops), 0);
    } else 
      return Op;
  }

  // If scalarType == sub-reg (e.g., f16), split it to multiple reg-BUILD_VECTOR
  // e.g., t2 : v4f16 = BUILD_VECTOR f16, f16, f16, f16 
  //  ==>
  // t0 : v2f16 = BUILD_VECTOR f16, f16 (Selected as PRMT ... 0x5410 ...;)
  // t1 : v2f16 = BUILD_VECTOR f16, f16
  // t2 : v4f16 = CONCAT_VECTORS t0:v2f16, t1:v2f16
  if (VT == MVT::v4f16 || VT == MVT::v8f16) {
    SmallVector<SDValue, 4> Ops;

    // create basic values
    if (VT == MVT::v4f16) {
      unsigned RegClassID = GASS::VReg64RegClassID;
      Ops.push_back(DAG.getTargetConstant(RegClassID, DL, MVT::i32));
      SDValue Op0 = Op.getOperand(0);
      SDValue Op1 = Op.getOperand(1);
      SDValue Op2 = Op.getOperand(2);
      SDValue Op3 = Op.getOperand(3);
      SDValue T0 = DAG.getNode(ISD::BUILD_VECTOR, DL, MVT::v2f16, {Op0, Op1});
      SDValue T1 = DAG.getNode(ISD::BUILD_VECTOR, DL, MVT::v2f16, {Op2, Op3});
      Ops.push_back(T0);
      Ops.push_back(DAG.getTargetConstant(GASS::sub0, DL, MVT::i32));
      Ops.push_back(T1);
      Ops.push_back(DAG.getTargetConstant(GASS::sub1, DL, MVT::i32));
      return SDValue(
        DAG.getMachineNode(TargetOpcode::REG_SEQUENCE, DL, MVT::v4f16, Ops), 0);
    } else if (VT == MVT::v8f16) {
      unsigned RegClassID = GASS::VReg128RegClassID;
      Ops.push_back(DAG.getTargetConstant(RegClassID, DL, MVT::i32));
      SDValue Op0 = Op.getOperand(0);
      SDValue Op1 = Op.getOperand(1);
      SDValue Op2 = Op.getOperand(2);
      SDValue Op3 = Op.getOperand(3);
      SDValue Op4 = Op.getOperand(4);
      SDValue Op5 = Op.getOperand(5);
      SDValue Op6 = Op.getOperand(6);
      SDValue Op7 = Op.getOperand(7);
      SDValue T0 = DAG.getNode(ISD::BUILD_VECTOR, DL, MVT::v2f16, {Op0, Op1});
      SDValue T1 = DAG.getNode(ISD::BUILD_VECTOR, DL, MVT::v2f16, {Op2, Op3});
      SDValue T2 = DAG.getNode(ISD::BUILD_VECTOR, DL, MVT::v2f16, {Op4, Op5});
      SDValue T3 = DAG.getNode(ISD::BUILD_VECTOR, DL, MVT::v2f16, {Op6, Op7});
      Ops.push_back(T0);
      Ops.push_back(DAG.getTargetConstant(GASS::sub0, DL, MVT::i32));
      Ops.push_back(T1);
      Ops.push_back(DAG.getTargetConstant(GASS::sub1, DL, MVT::i32));
      Ops.push_back(T2);
      Ops.push_back(DAG.getTargetConstant(GASS::sub2, DL, MVT::i32));
      Ops.push_back(T3);
      Ops.push_back(DAG.getTargetConstant(GASS::sub3, DL, MVT::i32));
      return SDValue(
        DAG.getMachineNode(TargetOpcode::REG_SEQUENCE, DL, MVT::v8f16, Ops), 0);
    } else
      return Op;
  }
  return Op;
}

SDValue
GASSTargetLowering::lowerAddrSpaceCast(SDValue Op, SelectionDAG &DAG) const {
  SDLoc dl(Op);
  SDValue Src = Op.getOperand(0);
  MVT DstVT = Op.getSimpleValueType(); // Which is?

  // Always truncate
  Op = DAG.getNode(ISD::TRUNCATE, dl, DstVT, Src);

  return Op;
}

// Follow the practice in RISCV
SDValue GASSTargetLowering::lowerGlobalAddress(SDValue Op, 
                                               SelectionDAG &DAG) const {
  SDLoc DL(Op);
  GlobalAddressSDNode *GAN = cast<GlobalAddressSDNode>(Op);
  MVT PtrVT = getPointerTy(DAG.getDataLayout(), GAN->getAddressSpace());
  int64_t Offset = GAN->getOffset();

  SDValue Addr = getAddr(GAN, DAG);

  if (Offset != 0)
    return DAG.getNode(ISD::ADD, DL, PtrVT, Addr,
                       DAG.getConstant(Offset, DL, PtrVT));

  return Addr;
}

// By default CONCAT_VECTORS is lowered by ExpandVectorBuildThroughStack()
// (see LegalizeDAG.cpp). This is slow and uses local memory.
// We use extract/insert/build vector just as what LegalizeOp() does in llvm 2.5
SDValue
GASSTargetLowering::lowerCONCAT_VECTORS(SDValue Op, SelectionDAG &DAG) const {
  outs() << "lowering concat_vectors\n";
  SmallVector<SDValue, 8> Args;

  EVT VT = Op.getValueType();
  if (VT == MVT::v4i16 || VT == MVT::v4f16) {
    SDLoc SL(Op);
    SDValue Lo = DAG.getNode(ISD::BITCAST, SL, MVT::i32, Op.getOperand(0));
    SDValue Hi = DAG.getNode(ISD::BITCAST, SL, MVT::i32, Op.getOperand(1));

    SDValue BV = DAG.getBuildVector(MVT::v2i32, SL, { Lo, Hi });
    return DAG.getNode(ISD::BITCAST, SL, VT, BV);
  }

  for (const SDUse &U : Op->ops())
    DAG.ExtractVectorElements(U.get(), Args);

  return DAG.getBuildVector(Op.getValueType(), SDLoc(Op), Args);
}
//=--------------------------------------=//
// Custom i64 lowering
//=--------------------------------------=//
/// Build an integer with low bits Lo and high bits Hi.
SDValue GASSTargetLowering::JoinIntegers(SDValue Lo, SDValue Hi, 
                                         SelectionDAG &DAG) const {
  // Arbitrarily use dlHi for result SDLoc
  SDLoc dlHi(Hi);
  SDLoc dlLo(Lo);
  EVT LVT = Lo.getValueType();
  EVT HVT = Hi.getValueType();
  EVT NVT = EVT::getIntegerVT(*DAG.getContext(),
                              LVT.getSizeInBits() + HVT.getSizeInBits());

  EVT ShiftAmtVT = getShiftAmountTy(NVT, DAG.getDataLayout(), false);
  Lo = DAG.getNode(ISD::ZERO_EXTEND, dlLo, NVT, Lo);
  Hi = DAG.getNode(ISD::ANY_EXTEND, dlHi, NVT, Hi);
  Hi = DAG.getNode(ISD::SHL, dlHi, NVT, Hi,
                   DAG.getConstant(LVT.getSizeInBits(), dlHi, ShiftAmtVT));
  return DAG.getNode(ISD::OR, dlHi, NVT, Lo, Hi);
}

void GASSTargetLowering::SplitInteger(SDValue Op, EVT LoVT, EVT HiVT,
                                      SDValue &Lo, SDValue &Hi, 
                                      SelectionDAG &DAG) const {
  SDLoc dl(Op);
  assert(LoVT.getSizeInBits() + HiVT.getSizeInBits() ==
         Op.getValueSizeInBits() && "Invalid integer splitting!");
  Lo = DAG.getNode(ISD::TRUNCATE, dl, LoVT, Op);
  unsigned ReqShiftAmountInBits =
      Log2_32_Ceil(Op.getValueType().getSizeInBits());
  MVT ShiftAmountTy =
      getScalarShiftAmountTy(DAG.getDataLayout(), Op.getValueType());
  if (ReqShiftAmountInBits > ShiftAmountTy.getSizeInBits())
    ShiftAmountTy = MVT::getIntegerVT(NextPowerOf2(ReqShiftAmountInBits));
  Hi = DAG.getNode(ISD::SRL, dl, Op.getValueType(), Op,
                   DAG.getConstant(LoVT.getSizeInBits(), dl, ShiftAmountTy));
  Hi = DAG.getNode(ISD::TRUNCATE, dl, HiVT, Hi);
}

/// Return the lower and upper halves of Op's bits in a value type half the
/// size of Op's.
void GASSTargetLowering::SplitInteger(SDValue Op,
                                      SDValue &Lo, SDValue &Hi, 
                                      SelectionDAG &DAG) const {
  EVT HalfVT =
      EVT::getIntegerVT(*DAG.getContext(), Op.getValueSizeInBits() / 2);
  SplitInteger(Op, HalfVT, HalfVT, Lo, Hi, DAG);
}

// Expand int (ref: "TargetLowering::expandMUL_LOHI")
// i64 add -> i32 addc + i32 adde
SDValue GASSTargetLowering::lowerADD64(SDValue Op, SelectionDAG &DAG) const {
  SDLoc dl(Op);
  EVT VT = Op.getValueType();
  EVT HalfVT =
      EVT::getIntegerVT(*DAG.getContext(), Op.getValueSizeInBits() / 2);
  SDValue LHS = Op.getOperand(0);
  SDValue RHS = Op.getOperand(1);

  // Expand the subcomponents.
  SDValue LLo, LHi, RLo, RHi; // lhs-lo, ...

  SplitInteger(LHS, LLo, LHi, DAG);
  SplitInteger(RHS, RLo, RHi, DAG);

  SDValue Lo = SDValue();
  SDValue Hi = SDValue();

  // ADDCARRY
  SDValue Next;
  Next = DAG.getNode(ISD::ADDCARRY, dl, DAG.getVTList(HalfVT, MVT::i1), 
                     LLo, RLo, DAG.getConstant(0, dl, MVT::i1));
  Lo = Next.getValue(0);
  SDValue Carry = Next.getValue(1);

  Next = DAG.getNode(ISD::ADDCARRY, dl, DAG.getVTList(HalfVT, MVT::i1), 
                     LHi, RHi, Carry);
  Hi = Next.getValue(0);

  return JoinIntegers(Lo, Hi, DAG);
}

/// @param N The GAN Node.
SDValue GASSTargetLowering::getAddr(GlobalAddressSDNode *GAN, 
                                    SelectionDAG &DAG) const {
  assert(GAN->getAddressSpace() == GASS::SHARED && "Can only handle smem");

  const GlobalValue *GV = GAN->getGlobal();
  assert(isa<GlobalVariable>(*GV));

  
  SDLoc DL(GAN);
  // FIXME: this seems wrong?
  return DAG.getConstant(0, DL, MVT::i32);
}