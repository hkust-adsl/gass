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
FunctionPass *createGASSCodeGenPreparePass();
FunctionPass *createGASSBarrierSettingPass();
FunctionPass *createGASSStallSettingPass(); 
FunctionPass *createGASSExpandPreRAPseudoPass();

void initializeGASSBarrierSettingPass(PassRegistry &);
void initializeGASSStallSettingPass(PassRegistry&);
void initializeGASSExpandPreRAPseudoPass(PassRegistry&);
void initializeGASSCodeGenPreparePass(PassRegistry&);


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

// SETCC
namespace GASSCC {
enum CondCode {
  EQ = 0,
  NE = 1,
  LT = 2,
  LE = 3,
  GT = 4,
  GE = 5,

  EQU = 10,
  NEU = 11,
  LTU = 12,
  LEU = 13,
  GTU = 14,
  GEU = 15,
  NUM = 16,
  NaN = 17,
  
  LO,
  LS,
  HI,
  HS,
};
} // namespace GASSCC

namespace SHF_FLAGS {
enum LR {
  L,
  R
};

enum LOHI {
  LO,
  HI
};

enum TYPE {
  S32,
  U32,
  S64,
  U64
};
} // namespace SHF_FLAGS

} // namespace GASS

} // namespace llvm

#endif