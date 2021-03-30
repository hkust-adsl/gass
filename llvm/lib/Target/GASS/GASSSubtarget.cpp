#include "GASSSubtarget.h"
#include "llvm/IR/IntrinsicsNVPTX.h"
#include "llvm/Support/ErrorHandling.h"

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

bool GASSSubtarget::enableMachineScheduler() const { return true; }

bool GASSSubtarget::enablePostRAScheduler() const { return false; }

bool GASSSubtarget::enableEarlyIfConversion() const { return true; }

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

unsigned GASSSubtarget::getConstantOffset(unsigned IntNo) const {
  switch (SmVersion) {
  default: llvm_unreachable("SmVersion invalid");
  case 70:
  case 75:
  case 80:
  case 86: {
    switch (IntNo) {
    default: llvm_unreachable("Invalid intrinsic");
    case Intrinsic::nvvm_read_ptx_sreg_ntid_x: return 0x0;
    case Intrinsic::nvvm_read_ptx_sreg_ntid_y: return 0x4;
    case Intrinsic::nvvm_read_ptx_sreg_ntid_z: return 0x8;
    case Intrinsic::nvvm_read_ptx_sreg_nctaid_x: return 0xc;
    case Intrinsic::nvvm_read_ptx_sreg_nctaid_y: return 0x10;
    case Intrinsic::nvvm_read_ptx_sreg_nctaid_z: return 0x14;
    }
  } break;
  }
}