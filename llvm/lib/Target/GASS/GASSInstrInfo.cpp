#include "GASSInstrInfo.h"
#include "MCTargetDesc/GASSMCTargetDesc.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "GASSGenInstrInfo.inc"

bool GASSInstrInfo::isLoad(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  default:
    break;
  case GASS::LDC32c: case GASS::LDC64c:
  case GASS::LDG32r: case GASS::LDG32ri:
  case GASS::LDG64r: case GASS::LDG64ri:
    return true;
  }
  return false;
}

bool GASSInstrInfo::isStore(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
  default:
    break;
  case GASS::STG32r: case GASS::STG32ri:
  case GASS::STG64r: case GASS::STG64ri:
    return true;
  }
  return false;
}

MachineOperand* GASSInstrInfo::getMemOperandReg(MachineInstr &MI) {
  MachineOperand *Ret = nullptr;
  switch (MI.getOpcode()) {
  default: return Ret;
  case GASS::LDG32r: case GASS::LDG32ri:
    return &MI.getOperand(1);
  case GASS::STG32r: case GASS::STG32ri:
  case GASS::STG64r: case GASS::STG64ri:
    return &MI.getOperand(0);
  }
}

//=------------------------------------------------=//
// Encoding info (sm_70 ~ )
//=------------------------------------------------=//
// uint16_t flags; // 3+3+6+4 
// Wait Mask (6 bits) :: Read Barrier Idx (3 bits) :: 
// Write Barrier Idx (3 bits) :: Stall Cycles (4 bits)
void GASSInstrInfo::encodeReadBarrier(MachineInstr &MI, unsigned BarIdx) {
  assert(BarIdx < 6 && "Read Barrier should be smaller than 6");
  uint16_t Flags = MI.getFlags();
  Flags &= ~(0b111 << 7); // clear default value
  Flags |= BarIdx << 7;
  MI.setFlags(Flags);
}

void GASSInstrInfo::encodeWriteBarrier(MachineInstr &MI, unsigned BarIdx) {
  assert(BarIdx < 6 && "Write barrier should be smaller than 6");
  uint16_t Flags = MI.getFlags();
  Flags &= ~(0b111 << 4); // clear default value
  Flags |= BarIdx << 4;
  MI.setFlags(Flags);
}

void GASSInstrInfo::encodeBarrierMask(MachineInstr &MI, unsigned BarMask) {
  assert(BarMask < 64 && "Wait barrier mask should be smaller than 64");
  uint16_t Flags = MI.getFlags();
  Flags &= ~(0b11'1111 << 10);
  Flags |= BarMask << 10;
  MI.setFlags(Flags);
}

void GASSInstrInfo::encodeStallCycles(MachineInstr &MI, unsigned Stalls) {
  assert(Stalls < 16 && "Stall cycles should be smaller than 16");
  uint16_t Flags = MI.getFlags();
  Flags &= ~(0b1111);
  Flags |= Stalls;
  MI.setFlags(Flags);
}

void GASSInstrInfo::initializeFlagsEncoding(MachineInstr &MI) {
  // Default value:
  // Read & Write Barrier idx = 0b111;
  uint16_t Flags = MI.getFlags();
  Flags |= 0b0000'0011'1111'0000;
  MI.setFlags(Flags);
}