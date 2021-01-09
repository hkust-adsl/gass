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
  default:
    break;
  }
  // Tablegen'erated
  SelectCode(N);
}

bool GASSDAGToDAGISel::tryLoad(SDNode *N) {
  SDLoc dl(N);
  MemSDNode *LD = cast<MemSDNode>(N);
  assert(LD->readMem() && "Expected load");
  EVT LoadedVT = LD->getMemoryVT();

  unsigned AddrSpace = getCodeAddrSpace(LD);


  return false;
}

bool GASSDAGToDAGISel::tryStore(SDNode *N) {
  SDLoc dl(N);
  MemSDNode *ST = cast<MemSDNode>(N);
  assert(ST->writeMem() && "Expected store");
  EVT StoreVT = ST->getMemoryVT();
  SDNode *GASSST = nullptr;
  unsigned Opcode;

  unsigned AddrSpace = getCodeAddrSpace(ST);

  // Create the machine instruction DAG
  SDValue Chain = ST->getChain();
  SDValue Value = ST->getValue(); // What's value?
  SDValue BasePtr = ST->getBasePtr();
  SDValue Addr;
  SDValue Offset, Base;

  if (AddrSpace == GASS::GENERIC) {
    llvm_unreachable("GENERIC Store not implemented");
  } else if (AddrSpace == GASS::GLOBAL) {
    Opcode = GASS::STG32_r;
    SDValue Ops[] = {Value,
                     Addr,
                     Chain};
    GASSST = CurDAG->getMachineNode(Opcode, dl, MVT::Other, Ops);
  } else if (AddrSpace == GASS::SHARED) {
    llvm_unreachable("SHARED Store not implemented");
  } else if (AddrSpace == GASS::LOCAL) {
    llvm_unreachable("LOCAL Store not implemented");
  }

  ReplaceNode(N, GASSST);
  return true;
}