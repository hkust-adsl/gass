#include "GASSSubtarget.h"

using namespace llvm;

#define DEBUG_TYPE "gass-subtarget"

#define GET_SUBTARGETINFO_ENUM
#define GET_SUBTARGETINFO_TARGET_DESC
#define GET_SUBTARGETINFO_CTOR
#include "GASSGenSubtargetInfo.inc"

GASSSubtarget::GASSSubtarget(const Triple &TT, StringRef CPU, StringRef FS,
                             const TargetMachine &TM)
  : GASSGenSubtargetInfo(TT, CPU, CPU, FS),
    TLInfo(TM, *this) {
    // Provide the default CPU if we don't have one.
    std::string TargetName = std::string(CPU.empty() ? "sm_70" : CPU);

    ParseSubtargetFeatures(TargetName, /*TuneCPU*/ TargetName, FS);
}

unsigned GASSSubtarget::getParamBase() const {
  switch (SmVersion) {
  default:
    llvm_unreachable("SmVersion invalid");
  case 70:
  case 75:
  case 80:
    return 0x160;
  }
}