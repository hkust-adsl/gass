#include "GASS.h"
#include "GASSISelDAGToDAG.h"
#include "GASSSubtarget.h"
#include "MCTargetDesc/GASSMCTargetDesc.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IntrinsicsNVPTX.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "gass-isel"

bool GASSDAGToDAGISel::runOnMachineFunction(MachineFunction &MF) {
  Subtarget = &MF.getSubtarget<GASSSubtarget>();
  return SelectionDAGISel::runOnMachineFunction(MF);
}

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
    if (tryBUILD_VECTOR(N))
      return;
  } break;
  case ISD::EXTRACT_SUBVECTOR:
    if (tryEXTRACT_SUBVECTOR(N))
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
      break;
    case Intrinsic::nvvm_ldmatrix_m8n8_x4: case Intrinsic::nvvm_ldmatrix_m8n8_x4_trans: {
      // TODO: using tablegen
      // try ldmatrix
      SDLoc DL(N);
      assert(N->getNumValues() == 5); // v2f16x4 + chain
      const SDVTList &TargetVTs = N->getVTList();

      unsigned IfTrans = 0;
      if (IntNo == Intrinsic::nvvm_ldmatrix_m8n8_x4)
        IfTrans = 0;
      else 
        IfTrans = 1;

      SDValue Chain = N->getOperand(0);
      // Operand 1 is Intrinsic ID
      SDValue Ptr = N->getOperand(2);
      SDValue IOffset = N->getOperand(3);
      assert(isa<ConstantSDNode>(IOffset));
      SDValue IOff = CurDAG->getTargetConstant(
          cast<ConstantSDNode>(IOffset)->getSExtValue(), DL, MVT::i32);
            

      SDValue VOffset, UOffset;
      SDValue LdsmTrans = CurDAG->getTargetConstant(IfTrans, DL, MVT::i32);

      SDNode *Ldsm = nullptr;
      if (selectADDRrui(Ptr, VOffset, UOffset)) {
        SDValue Ops[] = {VOffset, UOffset, IOff, LdsmTrans, Chain};
        Ldsm = CurDAG->getMachineNode(GASS::LDSM_x4_rui_pseudo, DL, TargetVTs, Ops);
      } else {
        SDValue Ops[] = {Ptr, IOff, LdsmTrans, Chain};
        Ldsm = CurDAG->getMachineNode(GASS::LDSM_x4_ri_pseudo, DL, TargetVTs, Ops);
      }
      ReplaceNode(N, Ldsm);
      return;
    } break;
    }
  } break;
  case ISD::INTRINSIC_WO_CHAIN: {
    unsigned IntNo = cast<ConstantSDNode>(N->getOperand(0))->getZExtValue();
    switch (IntNo) {
    default: break;
    case Intrinsic::nvvm_read_ptx_sreg_ntid_x:
    case Intrinsic::nvvm_read_ptx_sreg_ntid_y:
    case Intrinsic::nvvm_read_ptx_sreg_ntid_z:
    case Intrinsic::nvvm_read_ptx_sreg_nctaid_x:
    case Intrinsic::nvvm_read_ptx_sreg_nctaid_y:
    case Intrinsic::nvvm_read_ptx_sreg_nctaid_z: {
      SDLoc DL(N);
      EVT TargetVT = N->getValueType(0);
      SDNode *GASSReadSreg = nullptr;

      unsigned ConstOffset = Subtarget->getConstantOffset(IntNo);
      SDValue Op = CurDAG->getConstant(ConstOffset, DL, MVT::i32);

      SDValue Ops[] = {Op, CurDAG->getRegister(GASS::PT, MVT::i1)};
      GASSReadSreg = CurDAG->getMachineNode(GASS::MOV32c, DL, TargetVT, Ops);
      ReplaceNode(N, GASSReadSreg);
      return;
    } break;
    }
  } break;
  case ISD::INTRINSIC_VOID: {
    unsigned IntNo = cast<ConstantSDNode>(N->getOperand(1))->getZExtValue();
    switch (IntNo) {
    default: break;
    // TODO: try to remove this
    case Intrinsic::nvvm_tput_bench:
      if (tryTPUT_BENCH(N))
        return;
    } break;
  } break;
  case ISD::ConstantFP: {
    if (N->getValueType(0).getSizeInBits() >= 32)
      break;
    
    uint32_t Imm;
    ConstantFPSDNode *FP = cast<ConstantFPSDNode>(N);
    Imm = FP->getValueAPF().bitcastToAPInt().getZExtValue();
    EVT TargetVT = N->getValueType(0);

    SDLoc DL(N);
    SDValue Ops[] = {CurDAG->getConstant(Imm, DL, MVT::i32), 
                   CurDAG->getConstant(0, DL, MVT::i32),
                   CurDAG->getRegister(GASS::PT, MVT::i1)};
    SDNode *MovImm = CurDAG->getMachineNode(GASS::MOV32i, DL, TargetVT, Ops);
    ReplaceNode(N, MovImm);
    return;
  } break;
  // GASS ISD
  case GASSISD::LDC:
    if (tryLDC(N))
      return;
    break;
  case GASSISD::SETCC_LOGIC: 
    if (trySETCC_LOGIC(N))
      return;
    break;
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

/// Return true if matches (vreg + ureg)
bool GASSDAGToDAGISel::selectADDRrui(SDValue Value, SDValue &VOffset, SDValue &UOffset) {
  return false; // Not implemented
  if (Value.getOpcode() == ISD::ADD) {
    SDValue LHS = Value.getOperand(0);
    SDValue RHS = Value.getOperand(1);

    if (!LHS->isDivergent()) {
      VOffset = RHS;
      UOffset = LHS;
      return true;
    } 
    if (!RHS->isDivergent()) {
      VOffset = LHS;
      UOffset = RHS;
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
  SDValue PredFlip = CurDAG->getTargetConstant(0, dl, MVT::i32);
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
      SDValue Ops[] = {Base, Offset, PredFlip, PredMask, Chain};
      GASSLD = CurDAG->getMachineNode(Opcode, dl, TargetVT, MVT::Other, Ops);
    } else { // default
      switch (ValueWidth) {
      default: llvm_unreachable("Invalid ld width");
      case 32: Opcode = GASS::LDG32r; break;
      case 64: Opcode = GASS::LDG64r; break;
      case 128: Opcode = GASS::LDG128r; break;
      }
      SDValue Ops[] = {N1, PredFlip, PredMask, Chain};
      GASSLD = CurDAG->getMachineNode(Opcode, dl, TargetVT, MVT::Other, Ops);
    }

  } else if (AddrSpace == GASS::SHARED) {
    if (selectADDRri(N1, Base, Offset)) {
      switch (ValueWidth) {
      default: llvm_unreachable("Invaild lds width");
      case 16: Opcode = GASS::LDS16ri; break;
      case 32: Opcode = GASS::LDS32ri; break;
      case 64: Opcode = GASS::LDS64ri; break;
      case 128: Opcode = GASS::LDS128ri; break;
      }
      SDValue Ops[] = {Base, Offset, PredFlip, PredMask, Chain};
      GASSLD = CurDAG->getMachineNode(Opcode, dl, TargetVT, MVT::Other, Ops);
    } else {
      switch (ValueWidth) {
      default: llvm_unreachable("Invalid lds width");
      case 16: Opcode = GASS::LDS16r; break;
      case 32: Opcode = GASS::LDS32r; break;
      case 64: Opcode = GASS::LDS64r; break;
      case 128: Opcode = GASS::LDS128r; break;
      }
      SDValue Ops[] = {N1, PredFlip, PredMask, Chain};
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
    Ops.push_back(CurDAG->getTargetConstant(0, dl, MVT::i32));
    Ops.push_back(CurDAG->getRegister(GASS::PT, MVT::i1));
    break;
  case 64: 
    Opcode = GASS::LDC64c;
    Ops.push_back(CurDAG->getTargetConstant(OffsetVal, dl, MVT::i64));
    Ops.push_back(CurDAG->getTargetConstant(0, dl, MVT::i32));
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
  SDValue PredFlip = CurDAG->getTargetConstant(0, dl, MVT::i32);
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
      SDValue Ops[] = {Value, Base, Offset, PredFlip, PredMask, Chain};
      GASSST = CurDAG->getMachineNode(Opcode, dl, MVT::Other, Ops);
    } else {
      switch (ValueWidth) {
      default: llvm_unreachable("Invalid stg width");
      case 32: Opcode = GASS::STG32r; break;
      case 64: Opcode = GASS::STG64r; break;
      case 128: Opcode = GASS::STG128r; break;
      }
      SDValue Ops[] = {Value,
                       BasePtr, PredFlip, PredMask, Chain}; // Should we pass chain as op?
      GASSST = CurDAG->getMachineNode(Opcode, dl, MVT::Other, Ops);
    }

  } else if (AddrSpace == GASS::SHARED) {
    if (selectADDRri(BasePtr, Base, Offset)) {
      switch (ValueWidth) {
      default: llvm_unreachable("Invalid sts width");
      case 16: Opcode = GASS::STS16ri; break;
      case 32: Opcode = GASS::STS32ri; break;
      case 64: Opcode = GASS::STS64ri; break;
      case 128: Opcode = GASS::STS128ri; break;
      }
      SDValue Ops[] = {Value, Base, Offset, PredFlip, PredMask, Chain};
      GASSST = CurDAG->getMachineNode(Opcode, dl, MVT::Other, Ops);
    } else {
      switch (ValueWidth) {
      default: llvm_unreachable("Invalid sts width");
      case 16: Opcode = GASS::STS16r; break;
      case 32: Opcode = GASS::STS32r; break;
      case 64: Opcode = GASS::STS64r; break;
      case 128: Opcode = GASS::STS128r; break;
      }
      SDValue Ops[] = {Value, BasePtr, PredFlip, PredMask, Chain};
      GASSST = CurDAG->getMachineNode(Opcode, dl, MVT::Other, Ops);
    }
  } else if (AddrSpace == GASS::LOCAL) {
    llvm_unreachable("LOCAL Store not implemented");
  }

  ReplaceNode(N, GASSST);
  return true;
}

bool GASSDAGToDAGISel::tryEXTRACT_VECTOR_ELT_F16(SDNode *N) {
  // def : Pat<(f16 (extractelt v8f16:$src, 0)), (EXTRACT_SUBREG v8f16:$src, lo)>;
  // Note: We need c++ since tablegen doesn't have sub1_then_lo
  SDLoc DL(N);
  SDValue Vector = N->getOperand(0);
  SDValue Idx = N->getOperand(1);
  assert(isa<ConstantSDNode>(Idx));

  unsigned IdxValue = dyn_cast<ConstantSDNode>(Idx)->getSExtValue();
  unsigned SubRegValue = 0;
  switch (IdxValue) {
  default: llvm_unreachable("error");
  case 0: SubRegValue = GASS::lo; break;
  case 1: SubRegValue = GASS::hi; break;
  case 2: SubRegValue = GASS::sub1_then_lo; break;
  case 3: SubRegValue = GASS::sub1_then_hi; break;
  case 4: SubRegValue = GASS::sub2_then_lo; break;
  case 5: SubRegValue = GASS::sub2_then_hi; break;
  case 6: SubRegValue = GASS::sub3_then_lo; break;
  case 7: SubRegValue = GASS::sub3_then_hi; break;
  }

  SDValue SubReg = CurDAG->getTargetConstant(SubRegValue, DL, MVT::i32);

  MVT ResultTy = N->getSimpleValueType(0);

  SDValue Ops[] = {Vector, SubReg};
  SDNode *ExtraSubReg = CurDAG->getMachineNode(TargetOpcode::EXTRACT_SUBREG,
                                               DL, ResultTy, Ops);

  ReplaceNode(N, ExtraSubReg);
  return true;
}

bool GASSDAGToDAGISel::tryEXTRACT_VECTOR_ELT(SDNode *N) {
  // Replace extract_vector_elt with extract_subreg
  SDLoc DL(N);
  SDNode *ExtraSubReg = nullptr;

  SDValue Vector = N->getOperand(0);
  SDValue Idx = N->getOperand(1);

  MVT VectorTy = Vector.getSimpleValueType();
  // XXX: Sub-reg types
  MVT ScalarTy = VectorTy.getScalarType();
  if (ScalarTy == MVT::f16 || 
      ScalarTy == MVT::i16 || ScalarTy == MVT::i8) {
    assert(ScalarTy == MVT::f16 && "Only support f16 now");
    return false;
    // XXX: Now use TableGen
    // return tryEXTRACT_VECTOR_ELT_F16(N);
  }
  // FIXME: we should support more types
  if (!(VectorTy == MVT::v2f32 || VectorTy == MVT::v2i32 ||
         VectorTy == MVT::v4f32 || VectorTy == MVT::v4f32))
    return false;
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

bool GASSDAGToDAGISel::tryEXTRACT_SUBVECTOR(SDNode *N) {
  // Select EXTRACT_SUBVECTOR as TargetOpcode::EXTRACT_SUBREG
  assert(N->getOpcode() == ISD::EXTRACT_SUBVECTOR);

  SDLoc DL(N);
  SDNode *MachineNode = nullptr;

  // Now cannot handle sub-reg (less than 32 bits)
  MVT VT = N->getSimpleValueType(0);
  assert(VT.getFixedSizeInBits() % 32 == 0 && "Cannot handle sub-reg");

  SDValue Src = N->getOperand(0);
  SDValue StartIdx = N->getOperand(1);
  unsigned IdxValue = dyn_cast<ConstantSDNode>(StartIdx)->getSExtValue();

  MVT SrcTy = Src.getSimpleValueType();
  
  if (SrcTy.getFixedSizeInBits() == 64) {
    // 64-bit src    
    llvm_unreachable("Not implemented");
  } else if (SrcTy.getFixedSizeInBits() == 128) {
    // 128-bit src
    if (VT.getFixedSizeInBits() == 32) {
      llvm_unreachable("Not implemented");
    } else if (VT.getFixedSizeInBits() == 64) {
      // 2 EXTRACT_SUBREG + REG_SEQUENCE
      // (sub0, sub1) or (sub2, sub3)
      unsigned BitsOffset = VT.getScalarSizeInBits() * IdxValue;
      assert(BitsOffset == 0 || BitsOffset == 64);
      // TODO: merge these two cases.
      if (BitsOffset == 0) {
        // sub0, sub1
        SDValue Sub0Ops[] = 
          {Src, CurDAG->getTargetConstant(GASS::sub0, DL, MVT::i32)};
        SDValue Sub0 = SDValue(
          CurDAG->getMachineNode(TargetOpcode::EXTRACT_SUBREG, DL, MVT::i32, 
                                 Sub0Ops), 0);
        SDValue Sub1Ops[] = 
          {Src, CurDAG->getTargetConstant(GASS::sub1, DL, MVT::i32)};
        SDValue Sub1 = SDValue(
          CurDAG->getMachineNode(TargetOpcode::EXTRACT_SUBREG, DL, MVT::i32, 
                                 Sub1Ops), 0);
        SDValue Ops[] = 
          {CurDAG->getTargetConstant(GASS::VReg64RegClassID, DL, MVT::i32), 
           Sub0, 
           CurDAG->getTargetConstant(GASS::sub0, DL, MVT::i32),
           Sub1,
           CurDAG->getTargetConstant(GASS::sub1, DL, MVT::i32)};
        MachineNode = 
          CurDAG->getMachineNode(TargetOpcode::REG_SEQUENCE, DL, N->getVTList(), 
                                 Ops);
      } else if (BitsOffset == 64) {
        // sub2, sub3
        SDValue Sub2Ops[] = 
          {Src, CurDAG->getTargetConstant(GASS::sub2, DL, MVT::i32)};
        SDValue Sub2 = SDValue(
          CurDAG->getMachineNode(TargetOpcode::EXTRACT_SUBREG, DL, MVT::i32, 
                                 Sub2Ops), 0);
        SDValue Sub3Ops[] = 
          {Src, CurDAG->getTargetConstant(GASS::sub3, DL, MVT::i32)};
        SDValue Sub3 = SDValue(
          CurDAG->getMachineNode(TargetOpcode::EXTRACT_SUBREG, DL, MVT::i32, 
                                 Sub3Ops), 0);
        SDValue Ops[] = 
          {CurDAG->getTargetConstant(GASS::VReg64RegClassID, DL, MVT::i32), 
           Sub2, 
           CurDAG->getTargetConstant(GASS::sub0, DL, MVT::i32),
           Sub3,
           CurDAG->getTargetConstant(GASS::sub1, DL, MVT::i32)};
        MachineNode = 
          CurDAG->getMachineNode(TargetOpcode::REG_SEQUENCE, DL, N->getVTList(), 
                                 Ops);
      }
    }
  } else
    llvm_unreachable("error");

  ReplaceNode(N, MachineNode);
  return true;
}

bool GASSDAGToDAGISel::tryBUILD_VECTOR(SDNode *N) {
    SDLoc DL(N);
    EVT VT = N->getValueType(0);
    unsigned NumVectorElts = VT.getVectorNumElements();
    unsigned VTWidth = VT.getFixedSizeInBits();
    assert(VTWidth % 32 == 0 && "Do not know how to select this BUILD_VECTOR,"
                                " should lower it first.");
    // RegClass created by REG_SEQUENCE
    unsigned RegClassID;
    switch (VTWidth) {
    default: llvm_unreachable("error");
    // TODO: shouldn't be here? (32 is weird)
    case 32: RegClassID = GASS::VReg32RegClassID; break;
    case 64: RegClassID = GASS::VReg64RegClassID; break;
    case 128: RegClassID = GASS::VReg128RegClassID; break;
    }

    SDNode *RegSeq = nullptr;
    SmallVector<SDValue, 4*2> Ops;
    // sub-reg BUILD_VECTOR is handled by Tablegen'd code.
    if (VT.getScalarSizeInBits() % 32 != 0) 
      return false;
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
    return true;
}

// TODO: remove this. This can be buggy.
bool GASSDAGToDAGISel::tryTPUT_BENCH(SDNode *N) {
  llvm_unreachable("Do not use llvm.nvvm.tput_bench");
  // SDValue Chain = N->getOperand(0);
  // SDValue StrNode = N->getOperand(2);
  // assert(isa<MDNodeSDNode>(StrNode));
  // const MDNode *MD = dyn_cast<MDNodeSDNode>(StrNode)->getMD();
  // StringRef OpcStr = dyn_cast<MDString>(MD->getOperand(0).get())->getString();
  // unsigned OpcSASS = StringSwitch<unsigned>(OpcStr)
  //   .CaseLower("hmma.16816", GASS::HMMA16816_f32_f16_f16_f32)
  //   .CaseLower("ffma", GASS::FFMArrr)
  //   .Default(GASS::INSTRUCTION_LIST_END);

  // SDLoc DL(N);
  // SDValue Ops[] = {CurDAG->getConstant(OpcSASS, DL, MVT::i32)};
  // SDNode *GASSTPUT_BENCH = CurDAG->getMachineNode(
  //     GASS::TPUT_BENCH_PSEUDO, DL, MVT::Other, Ops);
  // ReplaceNode(N, GASSTPUT_BENCH);
  // assert(OpcSASS != GASS::INSTRUCTION_LIST_END);
  // return true;
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

  SDValue PredFlip = CurDAG->getTargetConstant(0, DL, MVT::i32);
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
  SDValue Ops[] = {Val, LaneMask, Width, ShflMode, PredFlip, PredMask, Chain};
  GASSSHFL = CurDAG->getMachineNode(Opcode, DL, TargetVT, MVT::Other, Ops);
  ReplaceNode(N, GASSSHFL);
  return true;
}

bool GASSDAGToDAGISel::trySETCC_LOGIC(SDNode *N) {
  // SETCC_LOGIC:
  // (outs VReg1:$dst)
  // (ins VReg32:$src0, VReg32:$src1, VReg1:$psrc0, GASSCC, GASSCCSign, Logic)
  SDLoc DL(N);
  SDNode *GASSSETCC_LOGIC = nullptr;
  unsigned Opcode = 0;
  MVT VT = N->getSimpleValueType(0);
  assert(VT == MVT::i1);
  assert(N->getNumOperands() == 6);
  SDValue PredFlip = CurDAG->getTargetConstant(0, DL, MVT::i32);
  SDValue PredMask = CurDAG->getRegister(GASS::PT, MVT::i1);

  SDValue Op0 = N->getOperand(0);
  SDValue Op1 = N->getOperand(1);
  SDValue Op2 = N->getOperand(2);
  if (isa<ConstantSDNode>(Op1))
    Opcode = GASS::ISETPLogicri;
  else
    Opcode = GASS::ISETPLogicrr;

  SDValue CC = N->getOperand(3);
  SDValue Sign = N->getOperand(4);
  SDValue Logic = N->getOperand(5);
  assert(isa<ConstantSDNode>(CC) && isa<ConstantSDNode>(Sign) && 
         isa<ConstantSDNode>(Logic));
  
  SDValue CmpMode = CurDAG->getTargetConstant(
      dyn_cast<ConstantSDNode>(CC)->getZExtValue(), DL, MVT::i32);
  SDValue CmpSign = CurDAG->getTargetConstant(
      dyn_cast<ConstantSDNode>(Sign)->getSExtValue(), DL, MVT::i32);
  SDValue CmpLogic = CurDAG->getTargetConstant(
      dyn_cast<ConstantSDNode>(Logic)->getSExtValue(), DL, MVT::i32);
  SDValue Ops[] = {Op0, Op1, Op2, CmpMode, CmpSign, CmpLogic, 
                   PredFlip, PredMask};
  GASSSETCC_LOGIC = CurDAG->getMachineNode(Opcode, DL, VT, Ops);
  ReplaceNode(N, GASSSETCC_LOGIC);
  return true;
}