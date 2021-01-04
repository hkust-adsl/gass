#include "GASSMCTargetDesc.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"

using namespace llvm;

#define GET_INSTRINFO_MC_DESC
#include "GASSGenInstrInfo.inc"

#define GET_REGINFO_MC_DESC
#include "GASSGenRegisterInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "GASSGenSubtargetInfo.inc"

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeGASSTargetMC() {
  // 
}