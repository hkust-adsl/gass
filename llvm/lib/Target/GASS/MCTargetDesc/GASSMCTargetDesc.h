#ifndef LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSMCTARGETDESC_H
#define LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSMCTARGETDESC_H

#include "llvm/MC/MCTargetOptions.h"

namespace llvm {
class MCAsmBackend;
class MCCodeEmitter;
class MCInstrInfo;
class MCRegisterInfo;
class MCContext;
class Target;
class MCSubtargetInfo;

MCCodeEmitter *createGASSMCCodeEmitter(const MCInstrInfo &MCII,
                                       const MCRegisterInfo &MRI,
                                       MCContext &CTX);

MCAsmBackend *createGASSAsmBackend(const Target &T, const MCSubtargetInfo &STI,
                                   const MCRegisterInfo &MRI,
                                   const MCTargetOptions &Options);
} // namespace llvm

#define GET_REGINFO_ENUM
#include "GASSGenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#include "GASSGenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "GASSGenSubtargetInfo.inc"

#endif