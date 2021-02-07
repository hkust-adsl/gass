#include "GASS.h"
#include "GASSISelDAGToDAG.h"

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
  case GASSISD::LDC:
    if (tryLDC(N))
      return;
    break;
  default:
    break;
  }
  // Tablegen'erated
  SelectCode(N);
}

// helpers for load/store
/// Return true if matches
bool GASSDAGToDAGISel::selectDirectAddr(SDValue Value, SDValue &Addr) {
  // ld $dst, [$value];
  //  ==>
  // $Addr = $value
  // FIXME FIXME FIXME 
  // != ISD::ADD is not enough
  // better be 
  // Value != ISD::ADD || (Value.getOpcode() == ISD::ADD && ... != Constant...)
  if (Value.getOpcode() != ISD::ADD) {
    Addr = Value;
    return true;
  }
  return false;
}

/// Return true if matches
bool GASSDAGToDAGISel::selectADDRri(SDValue Value, SDValue &Base, SDValue &Offset) {
  // $value = add $base, $offset;
  // ld $dst, [$value];
  //   ==>
  // ld $dst, [$base+$offset];
  if (Value.getOpcode() == ISD::ADD) {
    if (auto *CN = dyn_cast<ConstantSDNode>(Value.getOperand(1))) {
      SDValue base = Value.getOperand(0);
      if (selectDirectAddr(base, Base)) {
        Offset = CurDAG->getTargetConstant(CN->getSExtValue(), SDLoc(Value), 
                                           MVT::i32);
        return true;
      }
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
  // Machine-DAG
  SDValue Addr;
  SDValue Base, Offset;

  if (AddrSpace == GASS::GENERIC) {
    llvm_unreachable("GENERIC load not implemented");
  } else if (AddrSpace == GASS::GLOBAL) {
    // FIXME FIXME FIXME 
    // Load width
    if (selectDirectAddr(N1, Addr)) {
      Opcode = GASS::LDG32r;
      SDValue Ops[] = {N1, Chain};
      GASSLD = CurDAG->getMachineNode(Opcode, dl, TargetVT, MVT::Other, Ops);
    } else if (selectADDRri(N1, Base, Offset)) {
      Opcode = GASS::LDG32ri;
      SDValue Ops[] = {Base, Offset, Chain};
      GASSLD = CurDAG->getMachineNode(Opcode, dl, TargetVT, MVT::Other, Ops);
    } else
      llvm_unreachable("shouldn't be here");

  } else if (AddrSpace == GASS::SHARED) {
    llvm_unreachable("SHARED load not implemented");
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
    break;
  case 64: 
    Opcode = GASS::LDC64c;
    Ops.push_back(CurDAG->getTargetConstant(OffsetVal, dl, MVT::i64));
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
  SDValue BasePtr = ST->getBasePtr(); // Ptr.
  SDValue Addr;
  SDValue Base, Offset;

  if (AddrSpace == GASS::GENERIC) {
    llvm_unreachable("GENERIC Store not implemented");
  } else if (AddrSpace == GASS::GLOBAL) {
    if (selectDirectAddr(BasePtr, Addr)) {
      Opcode = GASS::STG32r;
      SDValue Ops[] = {Value,
                       BasePtr, Chain}; // Should we pass chain as op?
      GASSST = CurDAG->getMachineNode(Opcode, dl, MVT::Other, Ops);
    } else if (selectADDRri(BasePtr, Base, Offset)) {
      Opcode = GASS::STG32ri;
      SDValue Ops[] = {Value, Base, Offset, Chain};
      GASSST = CurDAG->getMachineNode(Opcode, dl, MVT::Other, Ops);
    } else 
      llvm_unreachable("shouldn't be here");

  } else if (AddrSpace == GASS::SHARED) {
    llvm_unreachable("SHARED Store not implemented");
  } else if (AddrSpace == GASS::LOCAL) {
    llvm_unreachable("LOCAL Store not implemented");
  }

  ReplaceNode(N, GASSST);
  return true;
}