#include "GASSRegisterInfo.h"
#include "GASSSubtarget.h"
#include "GASSFrameLowering.h"
#include "MCTargetDesc/GASSMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunction.h"

#define GET_REGINFO_TARGET_DESC
#include "GASSGenRegisterInfo.inc"

using namespace llvm;

GASSRegisterInfo::GASSRegisterInfo() : GASSGenRegisterInfo(0) {}

//=------------overriden pure virtual function---------------------=//
// TODO: Revisit this.
const MCPhysReg *
GASSRegisterInfo::getCalleeSavedRegs(const MachineFunction *) const {
  static const MCPhysReg CalleeSavedRegs[] = {0};
  return CalleeSavedRegs;
}

BitVector GASSRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs()); // What's this?
  return Reserved;
}

void GASSRegisterInfo::eliminateFrameIndex(MachineBasicBlock::iterator II,
                                           int SPAdj, unsigned FIOperandNum,
                                           RegScavenger *RS) const {
  // TODO: fill this.
}

Register GASSRegisterInfo::getFrameRegister(const MachineFunction &MF) const {
  return GASS::VGPR1;
}