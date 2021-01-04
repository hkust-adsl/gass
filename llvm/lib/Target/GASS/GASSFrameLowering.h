#ifndef LLVM_LIB_TARGET_GASS_GASSFRAMELOWERING_H
#define LLVM_LIB_TARGET_GASS_GASSFRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {
class GASSFrameLowering : public TargetFrameLowering {
public:
  explicit GASSFrameLowering();

  // Override virtual functions
  bool hasFP(const MachineFunction &MF) const override { return true; }
  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override;
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override;

  // 
};
} // namespace llvm

#endif