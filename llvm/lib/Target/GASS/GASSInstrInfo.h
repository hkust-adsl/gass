#ifndef LLVM_LIB_TARGET_GASS_GASSINSTRINFO_H
#define LLVM_LIB_TARGET_GASS_GASSINSTRINFO_H

#include "GASSRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "GASSGenInstrInfo.inc"

namespace llvm {
class GASSSubtarget;

class GASSInstrInfo : public GASSGenInstrInfo {

};
} // namespace llvm

#endif