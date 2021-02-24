#include "GASS.h"
#include "GASSISelDAGToDAG.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/IR/IntrinsicsNVPTX.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "gass-isel"

//=-----------------------------------=//
// static helpers
//=-----------------------------------=//
static unsigned getCodeAddrSpace(MemSDNode *N) {
  const Value *Src = N->getMemOperand()->getValue();

  if (!Src)
    return GASS::GENERIC;

  if (auto *PT = dyn_cast<PointerType>(Src->getType())) {
    switch (PT->getAddressSpace()) {
    // TODO: change this to enum
    case 0: return GASS::GENERIC;
    case 1: return GASS::GLOBAL;
    case 3: return GASS::SHARED;
    case 4: return GASS::CONSTANT;
    case 5: return GASS::LOCAL;
    default: break;
    }
  }

  return GASS::GENERIC;
}

void GASSDAGToDAGISel::Select(SDNode *N) {
  if (N->isMachineOpcode()) {
    N->setNodeId(-1);
    return; // Already selected.
  }

  // Following the practice in NVPTX.
  switch (N->getOpcode()) {
  case ISD::LOAD:
    if (tryLoad(N))
      return;
    break;
  case ISD::STORE:
    if (tryStore(N))
      return;
    break;
  case ISD::EXTRACT_VECTOR_ELT:
    if (tryEXTRACT_VECTOR_ELT(N))
      return;
    break;
  case ISD::BUILD_VECTOR: {
    SDLoc DL(N);
    EVT VT = N->getValueType(0);
    unsigned NumVectorElts = VT.getVectorNumElements();
    unsigned RegClassID;

    switch(NumVectorElts) {
    case 2: RegClassID = GASS::VReg64RegClassID; break;
    case 4: RegClassID = GASS::VReg128RegClassID; break;
    default: llvm_unreachable("Do not know how to lower this BUILD_VECTOR");
    }

    SDNode *RegSeq = nullptr;
    SmallVector<SDValue, 4*2> Ops;
    Ops.push_back(CurDAG->getTargetConstant(RegClassID, DL, MVT::i32));
    for (unsigned i = 0; i < NumVectorElts; ++i) {
      unsigned Sub = 0;
      switch (i) {
      default: llvm_unreachable("error");
      case 0: Sub = GASS::sub0; break;
      case 1: Sub = GASS::sub1; break;
      case 2: Sub = GASS::sub2; break;
      case 3: Sub = GASS::sub3; break;
      }
      Ops.push_back(N->getOperand(i));
      Ops.push_back(CurDAG->getTargetConstant(Sub, DL, MVT::i32));
    }
    RegSeq = CurDAG->getMachineNode(TargetOpcode::REG_SEQUENCE, DL, 
                                    N->getVTList(), Ops);

    ReplaceNode(N, RegSeq);
    return;
  } break;
  case GASSISD::LDC:
    if (tryLDC(N))
      return;
    break;
  case ISD::INTRINSIC_W_CHAIN: {
    unsigned IntNo = cast<ConstantSDNode>(N->getOperand(1))->getZExtValue();
    switch (IntNo) {
    default: break;
    case Intrinsic::nvvm_shfl_sync_idx_i32: 
    case Intrinsic::nvvm_shfl_sync_idx_f32:
    case Intrinsic::nvvm_shfl_sync_up_i32: 
    case Intrinsic::nvvm_shfl_sync_up_f32:
    case Intrinsic::nvvm_shfl_sync_down_i32: 
    case Intrinsic::nvvm_shfl_sync_down_f32:
    case Intrinsic::nvvm_shfl_sync_bfly_i32: 
    case Intrinsic::nvvm_shfl_sync_bfly_f32:
      if (trySHFL(N))
        return;
    }
    break;
  }
  case ISD::INTRINSIC_WO_CHAIN: {
    unsigned IntNo = cast<ConstantSDNode>(N->getOperand(0))->getZExtValue();
    switch (IntNo) {
    default: break;
    // TODO: fill this
    }
    break;
  }

  default:
    break;
  }
  // Tablegen'erated
  SelectCode(N);
}

// Other helpers

// helpers for load/store
/// Return true if matches
bool GASSDAGToDAGISel::selectDirectAddr(SDValue Value, SDValue &Addr) {
  if (Value.getOpcode() == ISD::TargetGlobalAddress) {
    Addr = Value;
    return true;
  }
  return false;
}

/// Return true if matches (reg+offset)
bool GASSDAGToDAGISel::selectADDRri(SDValue Value, SDValue &Base, 
                                    SDValue &Offset) {
  // $value = add $base, $offset;
  // ld $dst, [$value];
  //   ==>
  // ld $dst, [$base+$offset];
  if (Value.getOpcode() == ISD::ADD) {
    if (auto *CN = dyn_cast<ConstantSDNode>(Value.getOperand(1))) {
      Base = Value.getOperand(0);
      // FIXME: should have if (selectDirectAddr()) ?
      Offset = CurDAG->getTargetConstant(CN->getSExtValue(), SDLoc(Value), 
                                          MVT::i32);
      return true;
    }
  }
  return false;
}

bool GASSDAGToDAGISel::tryLoad(SDNode *N) {
  SDLoc dl(N);
  MemSDNode *LD = cast<MemSDNode>(N);
  assert(LD->readMem() && "Expected load");
  EVT LoadedVT = LD->getMemoryVT();
  SDNode *GASSLD = nullptr;
  unsigned Opcode;

  unsigned AddrSpace = getCodeAddrSpace(LD);

  // IR-DAG
  SDValue Chain = N->getOperand(0);
  SDValue N1 = N->getOperand(1);
  MVT::SimpleValueType TargetVT = LD->getSimpleValueType(0).SimpleTy;
  unsigned ValueWidth = N->getValueSizeInBits(0);
  // Machine-DAG
  SDValue Addr;
  SDValue Base, Offset;
  SDValue PredMask = CurDAG->getRegister(GASS::PT, MVT::i1);

  if (AddrSpace == GASS::GENERIC) {
    llvm_unreachable("GENERIC load not implemented");
  } else if (AddrSpace == GASS::GLOBAL) {
    // Load width
    if (selectADDRri(N1, Base, Offset)) {
      switch (ValueWidth) {
      default: llvm_unreachable("Invalid ld width");
      case 32: Opcode = GASS::LDG32ri; break;
      case 64: Opcode = GASS::LDG64ri; break;
      case 128: Opcode = GASS::LDG128ri; break;
      }
      SDValue Ops[] = {Base, Offset, PredMask, Chain};
      GASSLD = CurDAG->getMachineNode(Opcode, dl, TargetVT, MVT::Other, Ops);
    } else { // default
      switch (ValueWidth) {
      default: llvm_unreachable("Invalid ld width");
      case 32: Opcode = GASS::LDG32r; break;
      case 64: Opcode = GASS::LDG64r; break;
      case 128: Opcode = GASS::LDG128r; break;
      }
      SDValue Ops[] = {N1, PredMask, Chain};
      GASSLD = CurDAG->getMachineNode(Opcode, dl, TargetVT, MVT::Other, Ops);
    }

  } else if (AddrSpace == GASS::SHARED) {
    if (selectADDRri(N1, Base, Offset)) {
      switch (ValueWidth) {
      default: llvm_unreachable("Invaild lds width");
      case 32: Opcode = GASS::LDS32ri; break;
      case 64: Opcode = GASS::LDS64ri; break;
      case 128: Opcode = GASS::LDS128ri; break;
      }
      SDValue Ops[] = {Base, Offset, PredMask, Chain};
      GASSLD = CurDAG->getMachineNode(Opcode, dl, TargetVT, MVT::Other, Ops);
    } else {
      switch (ValueWidth) {
      default: llvm_unreachable("Invalid lds width");
      case 32: Opcode = GASS::LDS32r; break;
      case 64: Opcode = GASS::LDS64r; break;
      case 128: Opcode = GASS::LDS128r; break;
      }
      SDValue Ops[] = {N1, PredMask, Chain};
      GASSLD = CurDAG->getMachineNode(Opcode, dl, TargetVT, MVT::Other, Ops);
    }
  } else if (AddrSpace == GASS::LOCAL) {
    llvm_unreachable("LOCAL load not implemented");
  } else if (AddrSpace == GASS::CONSTANT) {
    llvm_unreachable("CONSTANT load not implemented");
  }

  ReplaceNode(N, GASSLD);
  return true;
}

bool GASSDAGToDAGISel::tryLDC(SDNode *N) {
  SDLoc dl(N);
  SDNode *GASSLDC = nullptr;
  unsigned Opcode;

  MVT TargetVT = N->getValueType(0).getSimpleVT();
  SDValue Offset = N->getOperand(0);
  unsigned Width = TargetVT.getFixedSizeInBits();

  unsigned OffsetVal = cast<ConstantSDNode>(Offset)->getZExtValue();

  SmallVector<SDValue, 1> Ops;
  switch (Width) {
  default: llvm_unreachable("Invalid LDC width");
  case 32: 
    Opcode = GASS::LDC32c;
    Ops.push_back(CurDAG->getTargetConstant(OffsetVal, dl, MVT::i32));
    Ops.push_back(CurDAG->getRegister(GASS::PT, MVT::i1));
    break;
  case 64: 
    Opcode = GASS::LDC64c;
    Ops.push_back(CurDAG->getTargetConstant(OffsetVal, dl, MVT::i64));
    Ops.push_back(CurDAG->getRegister(GASS::PT, MVT::i1));
    break;
  }

  GASSLDC = CurDAG->getMachineNode(Opcode, dl, TargetVT, Ops);

  ReplaceNode(N, GASSLDC);
  return true;
}

bool GASSDAGToDAGISel::tryStore(SDNode *N) {
  SDLoc dl(N);
  MemSDNode *ST = cast<MemSDNode>(N);
  assert(ST->writeMem() && "Expected store");
  StoreSDNode *PlainStore = dyn_cast<StoreSDNode>(N);
  EVT StoreVT = ST->getMemoryVT();
  SDNode *GASSST = nullptr;
  unsigned Opcode;

  unsigned AddrSpace = getCodeAddrSpace(ST);

  // Create the machine instruction DAG
  SDValue Chain = ST->getChain(); // Dependency. (Same as value?)
  SDValue Value = PlainStore->getValue(); // Value it reads (store).
  unsigned ValueWidth = Value.getValueSizeInBits();
  SDValue BasePtr = ST->getBasePtr(); // Ptr.
  SDValue Addr;
  SDValue Base, Offset;
  SDValue PredMask = CurDAG->getRegister(GASS::PT, MVT::i1);

  if (AddrSpace == GASS::GENERIC) {
    llvm_unreachable("GENERIC Store not implemented");
  } else if (AddrSpace == GASS::GLOBAL) {
    if (selectADDRri(BasePtr, Base, Offset)) {
      switch (ValueWidth) {
      default: llvm_unreachable("Invalid stg width");
      case 32: Opcode = GASS::STG32ri; break;
      case 64: Opcode = GASS::STG64ri; break;
      case 128: Opcode = GASS::STG128ri; break;
      }
      SDValue Ops[] = {Value, Base, Offset, PredMask, Chain};
      GASSST = CurDAG->getMachineNode(Opcode, dl, MVT::Other, Ops);
    } else {
      switch (ValueWidth) {
      default: llvm_unreachable("Invalid stg width");
      case 32: Opcode = GASS::STG32r; break;
      case 64: Opcode = GASS::STG64r; break;
      case 128: Opcode = GASS::STG128r; break;
      }
      SDValue Ops[] = {Value,
                       BasePtr, PredMask, Chain}; // Should we pass chain as op?
      GASSST = CurDAG->getMachineNode(Opcode, dl, MVT::Other, Ops);
    }

  } else if (AddrSpace == GASS::SHARED) {
    if (selectADDRri(BasePtr, Base, Offset)) {
      switch (ValueWidth) {
      default: llvm_unreachable("Invalid sts width");
      case 32: Opcode = GASS::STS32ri; break;
      case 64: Opcode = GASS::STS64ri; break;
      case 128: Opcode = GASS::STS128ri; break;
      }
      SDValue Ops[] = {Value, Base, Offset, PredMask, Chain};
      GASSST = CurDAG->getMachineNode(Opcode, dl, MVT::Other, Ops);
    } else {
      switch (ValueWidth) {
      default: llvm_unreachable("Invalid sts width");
      case 32: Opcode = GASS::STS32r; break;
      case 64: Opcode = GASS::STS64r; break;
      case 128: Opcode = GASS::STS128r; break;
      }
      SDValue Ops[] = {Value, BasePtr, PredMask, Chain};
      GASSST = CurDAG->getMachineNode(Opcode, dl, MVT::Other, Ops);
    }
  } else if (AddrSpace == GASS::LOCAL) {
    llvm_unreachable("LOCAL Store not implemented");
  }

  ReplaceNode(N, GASSST);
  return true;
}

bool GASSDAGToDAGISel::tryEXTRACT_VECTOR_ELT(SDNode *N) {
  // Replace extract_vector_elt with extract_subreg
  SDLoc DL(N);
  SDNode *ExtraSubReg = nullptr;

  SDValue Vector = N->getOperand(0);
  SDValue Idx = N->getOperand(1);

  MVT VectorTy = Vector.getSimpleValueType();
  // FIXME: we should support more types
  assert(VectorTy == MVT::v2f32 || VectorTy == MVT::v2i32 ||
         VectorTy == MVT::v4f32 || VectorTy == MVT::v4f32);
  assert(isa<ConstantSDNode>(Idx));
  unsigned IdxValue = dyn_cast<ConstantSDNode>(Idx)->getSExtValue();
  unsigned SubRegValue = 0;
  switch (IdxValue) {
  default: llvm_unreachable("error");
  case 0: SubRegValue = GASS::sub0; break;
  case 1: SubRegValue = GASS::sub1; break;
  case 2: SubRegValue = GASS::sub2; break;
  case 3: SubRegValue = GASS::sub3; break;
  }

  SDValue SubReg = CurDAG->getTargetConstant(SubRegValue, DL, MVT::i32);

  MVT ResultTy = N->getSimpleValueType(0);

  SDValue Ops[] = {Vector, SubReg};
  ExtraSubReg = CurDAG->getMachineNode(TargetOpcode::EXTRACT_SUBREG,
                                       DL, ResultTy, Ops);
  
  ReplaceNode(N, ExtraSubReg);
  return true;
}

static unsigned getOpcode3Op(SDValue &Src0, SDValue &Src1, SDValue &Src2,
                             SelectionDAG *CurDAG,
                             unsigned Oprrr, unsigned Oprri, 
                             unsigned Oprir, unsigned Oprii) {
  if (!isa<ConstantSDNode>(Src1) && !isa<ConstantSDNode>(Src2))
    return Oprrr;
  else if (!isa<ConstantSDNode>(Src1) && isa<ConstantSDNode>(Src2)) {
    SDLoc DL(Src2);
    Src2 = CurDAG->getTargetConstant(cast<ConstantSDNode>(Src2)->getZExtValue(), 
                                     DL, MVT::i32);
    return Oprri;
  }
  else if (isa<ConstantSDNode>(Src1) && !isa<ConstantSDNode>(Src2)) {
    SDLoc DL(Src1);
    Src1 = CurDAG->getTargetConstant(cast<ConstantSDNode>(Src1)->getZExtValue(), 
                                     DL, MVT::i32);
    return Oprir;
  }
  else if (isa<ConstantSDNode>(Src1) && isa<ConstantSDNode>(Src2)) {
    SDLoc DL1(Src1);
    SDLoc DL2(Src2);
    Src1 = CurDAG->getTargetConstant(cast<ConstantSDNode>(Src1)->getZExtValue(),
                                     DL1, MVT::i32);
    Src2 = CurDAG->getTargetConstant(cast<ConstantSDNode>(Src2)->getZExtValue(), 
                                     DL2, MVT::i32);
    return Oprii;
  }
  else
    llvm_unreachable("Invalid opcode pattern");
}

bool GASSDAGToDAGISel::trySHFL(SDNode *N) {
  // int_nvvm_shfl_sync_idx_i32 =>
  //   ins: Mask, Val, LaneMask, Width (31)
  // SHFLrrr
  //   ins: ShflMode, Val, LaneMask, Width
  SDLoc DL(N);
  unsigned Opcode;
  EVT TargetVT = N->getValueType(0);
  SDNode *GASSSHFL = nullptr;

  SDValue Chain = N->getOperand(0);
  unsigned IntNo = cast<ConstantSDNode>(N->getOperand(1))->getZExtValue();

  SDValue Mask = N->getOperand(2); // ignored
  SDValue Val = N->getOperand(3);
  SDValue LaneMask = N->getOperand(4);
  SDValue Width = N->getOperand(5);
  SDValue ShflMode;

  SDValue PredMask = CurDAG->getRegister(GASS::PT, MVT::i1);

  switch (IntNo) {
  llvm_unreachable("Invalid shfl intrinsic");
  case Intrinsic::nvvm_shfl_sync_idx_i32:
  case Intrinsic::nvvm_shfl_sync_idx_f32:
    ShflMode = CurDAG->getTargetConstant(GASS::ShflMode::IDX, DL, MVT::i32);
    break;
  case Intrinsic::nvvm_shfl_sync_up_i32:
  case Intrinsic::nvvm_shfl_sync_up_f32:
    ShflMode = CurDAG->getTargetConstant(GASS::ShflMode::UP, DL, MVT::i32);
    break;
  case Intrinsic::nvvm_shfl_sync_down_i32:
  case Intrinsic::nvvm_shfl_sync_down_f32:
    ShflMode = CurDAG->getTargetConstant(GASS::ShflMode::DOWN, DL, MVT::i32);
    break;
  case Intrinsic::nvvm_shfl_sync_bfly_i32:
  case Intrinsic::nvvm_shfl_sync_bfly_f32:
    ShflMode = CurDAG->getTargetConstant(GASS::ShflMode::BFLY, DL, MVT::i32);
    break;
  }

  // src0, src1, src2, ShflMode
  // var,  LaneMask, Width, 
  // Do we need chain?
    Opcode = getOpcode3Op(Val, LaneMask, Width, CurDAG,
                        GASS::SHFLrrr, GASS::SHFLrri, 
                        GASS::SHFLrir, GASS::SHFLrii);
  SDValue Ops[] = {Val, LaneMask, Width, ShflMode, PredMask, Chain};
  GASSSHFL = CurDAG->getMachineNode(Opcode, DL, TargetVT, MVT::Other, Ops);
  ReplaceNode(N, GASSSHFL);
  return true;
}