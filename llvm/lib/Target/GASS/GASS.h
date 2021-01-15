#ifndef LLVM_LIB_TARGET_GASS_GASS_H
#define LLVM_LIB_TARGET_GASS_GASS_H

#include "llvm/Support/CodeGen.h"
#include "llvm/PassRegistry.h"

namespace llvm {
class GASSTargetMachine;
class GASSSubtarget;
class MachineInstr;
class MCInst;
class FunctionPass;

// Required by AsmPrinter
void LowerToMCInst(const MachineInstr *MI, MCInst &Inst);

FunctionPass *createGASSAddrSpacePass();
FunctionPass *createGASSBarrierSettingPass();
FunctionPass *createGASSStallSettingPass(); 

void initializeGASSBarrierSettingPass(PassRegistry &);
void initializeGASSStallSettingPass(PassRegistry&);


namespace GASS {
struct CtrlInfo {
  unsigned Stalls = 0;
  unsigned WBMask = 0;
  unsigned ReadBarrier = 0;
  unsigned WriteBarrier = 0;
  unsigned Yield = 0;
};

// Following the practice in NVPTX
enum AddressSpace : unsigned {
  GENERIC = 0,
  GLOBAL = 1,
  CONSTANT = 2,
  SHARED = 3,
  PARAM = 4, // Remove this?
  LOCAL = 5
};
} // namespace GASS

} // namespace llvm

#endif