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
} // namespace llvm

#endif