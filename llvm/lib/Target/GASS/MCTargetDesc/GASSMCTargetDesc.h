#ifndef LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSMCTARGETDESC_H
#define LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSMCTARGETDESC_H

#include "llvm/MC/MCTargetOptions.h"

#define GET_REGINFO_ENUM
#include "GASSGenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#include "GASSGenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "GASSGenSubtargetInfo.inc"

#endif