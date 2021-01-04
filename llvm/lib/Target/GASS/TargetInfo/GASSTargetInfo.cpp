#include "TargetInfo/GASSTargetInfo.h"
#include "llvm/Support/TargetRegistry.h"
using namespace llvm;

Target &llvm::getTheGASSTarget() {
  static Target TheGASSTarget;
  return TheGASSTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeGASSTargetInfo() {
  RegisterTarget<Triple::gass> X(getTheGASSTarget(), "gass",
                                 "64-bit GASS-NVGPU", "GASS");
}