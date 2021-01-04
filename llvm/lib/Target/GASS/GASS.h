#ifndef LLVM_LIB_TARGET_GASS_GASS_H
#define LLVM_LIB_TARGET_GASS_GASS_H

#include "llvm/Support/CodeGen.h"

namespace llvm {
class GASSTargetMachine;
class GASSSubtarget;
class InstructionSelector;
class GASSRegisterBankInfo;

InstructionSelector 
*createGASSInstructionSelector(const GASSTargetMachine &TM,
                               const GASSSubtarget &STI,
                               const GASSRegisterBankInfo &RBI);
} // namespace llvm

#endif