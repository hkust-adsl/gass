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

  // Return true if Reg completely covers Other
  bool regsCover(const Register &Reg, const Register &Other,
                 const MachineRegisterInfo &MRI) const;
};

} // namespace llvm

#endif