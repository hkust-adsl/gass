#ifndef LLVM_LIB_TARGET_GASS_GASSREGISTERINFO_H
#define LLVM_LIB_TARGET_GASS_GASSREGISTERINFO_H

#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_REGINFO_HEADER
#include "GASSGenRegisterInfo.inc"

namespace llvm {

class GASSRegisterInfo : public GASSGenRegisterInfo {
public:
  GASSRegisterInfo();
  // override pure virtual functions
  const MCPhysReg *getCalleeSavedRegs(const MachineFunction *MF) const override;

  BitVector getReservedRegs(const MachineFunction &MF) const override;

  void eliminateFrameIndex(MachineBasicBlock::iterator MI, int SPAdj,
                           unsigned FIOperandNum,
                           RegScavenger *RS = nullptr) const override;
  Register getFrameRegister(const MachineFunction &MF) const override;
  // end of pure virtual functions

  // query register type
  bool isConstantPhysReg(MCRegister PhysReg) const override;

  // Return true if Reg completely covers Other
  bool regsCover(const Register &Reg, const Register &Other,
                 const MachineRegisterInfo &MRI) const;

  /// \returns A SGPR reg class with the same width as \p VRC
  const TargetRegisterClass*
  getEquivalentSGPRClass(const TargetRegisterClass *VRC) const;
};

} // namespace llvm

#endif