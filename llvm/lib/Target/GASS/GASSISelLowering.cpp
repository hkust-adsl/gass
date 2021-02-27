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
  addRegisterClass(MVT::v4f32, &GASS::VReg128RegClass);
  
  // ConstantFP are legal in GASS (*Must*, otherwise will be expanded to 
  //                                                   load<ConstantPool>)
  setOperationAction(ISD::ConstantFP, MVT::f64, Legal);
  setOperationAction(ISD::ConstantFP, MVT::f32, Legal);
  setOperationAction(ISD::ConstantFP, MVT::f16, Legal);

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

  computeRegisterProperties(Subtarget.getRegisterInfo());
}

const char *GASSTargetLowering::getTargetNodeName(unsigned Opcode) const {
  switch (Opcode) {
  default: return "Unknown Node Name";
  case GASSISD::Wrapper: return "GASSISD::Wrapper";
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