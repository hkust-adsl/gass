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

// A reserved register:
//  * is not allocatable
//  * is considered always live
//  * is ignored by liveness tracking
BitVector GASSRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs()); // What's this?

  // Constant registers
  Reserved.set(GASS::RZ16_LO);
  Reserved.set(GASS::RZ16_HI);
  Reserved.set(GASS::RZ16_1_LO);
  Reserved.set(GASS::RZ16_1_HI);
  Reserved.set(GASS::RZ32);
  Reserved.set(GASS::RZ32_1);
  Reserved.set(GASS::RZ64);
  Reserved.set(GASS::PT);

  Reserved.set(GASS::URZ32);
  Reserved.set(GASS::UPT);
  
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

// Query register type
bool GASSRegisterInfo::isConstantPhysReg(MCRegister PhysReg) const {
  switch (PhysReg) {
  default: return false;
  case GASS::PT:
  case GASS::RZ16_LO: case GASS::RZ16_HI: 
  case GASS::RZ16_1_LO: case GASS::RZ16_1_HI:
  case GASS::RZ32: case GASS::RZ32_1: case GASS::RZ64:
    return true;
  }
}

bool 
GASSRegisterInfo::regsCover(const Register &Reg, const Register &Other,
                            const MachineRegisterInfo &MRI) const {
  if (regsOverlap(Reg, Other) && 
      getRegSizeInBits(Reg, MRI) >= getRegSizeInBits(Other, MRI))
    return true;
  else
    return false;
}

const TargetRegisterClass* 
GASSRegisterInfo::getEquivalentSGPRClass(const TargetRegisterClass *VRC) const {
  unsigned Size = getRegSizeInBits(*VRC);
  switch (Size) {
  default: llvm_unreachable("Unsupported SGPR");
  case 1: return &GASS::SReg1RegClass;
  case 32: return &GASS::SReg32RegClass;
  }
}