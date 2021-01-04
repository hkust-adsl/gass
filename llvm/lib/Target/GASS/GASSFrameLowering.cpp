#include "GASSFrameLowering.h"

using namespace llvm;

GASSFrameLowering::GASSFrameLowering()
  : TargetFrameLowering(TargetFrameLowering::StackGrowsUp, Align(8), 0) {}

void GASSFrameLowering::emitPrologue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  // TODO: fill this.
}

void GASSFrameLowering::emitEpilogue(MachineFunction &MF,
                                     MachineBasicBlock &MBB) const {
  // TODO: fill this.
}