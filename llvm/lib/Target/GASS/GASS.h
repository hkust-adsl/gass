#ifndef LLVM_LIB_TARGET_GASS_GASS_H
#define LLVM_LIB_TARGET_GASS_GASS_H

#include "llvm/Support/CodeGen.h"

namespace llvm {
class GASSTargetMachine;
class GASSSubtarget;
class MachineInstr;
class MCInst;

// Required by AsmPrinter
void LowerToMCInst(const MachineInstr *MI, MCInst &Inst);

namespace GASS {
struct CtrlInfo {
  unsigned Stalls = 0;
  unsigned WBMask = 0;
  unsigned ReadBarrier = 0;
  unsigned WriteBarrier = 0;
  unsigned Yield = 0;
};
} // namespace GASS

} // namespace llvm

#endif