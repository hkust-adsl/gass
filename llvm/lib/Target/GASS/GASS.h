#ifndef LLVM_LIB_TARGET_GASS_GASS_H
#define LLVM_LIB_TARGET_GASS_GASS_H

#include "llvm/Support/CodeGen.h"
#include "llvm/PassRegistry.h"
#include "llvm/Pass.h"

namespace llvm {
class GASSTargetMachine;
class GASSSubtarget;
class MachineInstr;
class MCInst;
class FunctionPass;

// Required by AsmPrinter
void LowerToMCInst(const MachineInstr *MI, MCInst &Inst);

FunctionPass *createGASSSinkingPass();
FunctionPass *createGASSAnnotateUniformValues();
FunctionPass *createGASSAddrSpacePass();
FunctionPass *createGASSCodeGenPreparePass();
FunctionPass *createGASSMachineFunctionCFGPrinterPass();
FunctionPass *createGASSBarrierSettingPass();
FunctionPass *createGASSStallSettingPass(); 
FunctionPass *createGASSExpandPreRAPseudoPass();
FunctionPass *createGASSIfConversionPass();
FunctionPass *createGASSPreEmitPreparePass();
FunctionPass *createRegPressureComputePass();
FunctionPass *createGASSLDGSinkPass();
FunctionPass *createGASSDeleteDeadPHIsPass();
FunctionPass *createGASSMachineInstrCombinePass();
Pass *createGASSIVDebugPass();
// Delete dead label for WAW reg
FunctionPass *createGASSMarkUndeadPass();
void initializeGASSMarkUndeadPass(PassRegistry &);
// ...
FunctionPass *createGASSMachineDCEPass();
void initializeGASSMachineDCEPass(PassRegistry &);

FunctionPass *createGASSConstantMemPropagatePass();
void initializeGASSConstantMemPropagatePass(PassRegistry &);

void initializeGASSAnnotateUniformValuesPass(PassRegistry &);
void initializeGASSSinkingPass(PassRegistry &);
void initializeGASSBarrierSettingPass(PassRegistry &);
void initializeGASSStallSettingPass(PassRegistry&);
void initializeGASSExpandPreRAPseudoPass(PassRegistry&);
void initializeGASSCodeGenPreparePass(PassRegistry&);
void initializeGASSMachineFunctionCFGPrinterPass(PassRegistry&);
void initializeGASSIfConversionPass(PassRegistry&);
void initializeGASSBranchOffsetPass(PassRegistry &);
void initializeGASSPreEmitPreparePass(PassRegistry &);
void initializeRegPressureComputePass(PassRegistry &);
void initializeGASSLDGSinkPass(PassRegistry &);
void initializeGASSDeleteDeadPHIsPass(PassRegistry &);
void initializeGASSIVDebugPass(PassRegistry &);
void initializeGASSDAGToDAGISelPass(PassRegistry &);
void initializeGASSMachineInstrCombinePass(PassRegistry &);


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
enum CondCode : unsigned {
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

enum CondCodeSign : unsigned {
  U32 = 0,
  S32 = 1,
};

enum LogicOp : unsigned {
  AND = 0,
  XOR = 1,
  OR = 2,
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

namespace ShflMode {
enum MODE : unsigned {
  IDX,
  UP,
  DOWN,
  BFLY
};
} // namespace ShflMode

namespace TensorCore {
enum MmaLayout : unsigned {
  ROW = 0,
  COL = 1,
};

enum STEP : unsigned {
  STEP0 = 0,
  STEP1 = 1,
  STEP2 = 2,
  STEP3 = 3,
};
} // namespace TensorCore

namespace MufuFlag {
enum Flag : unsigned {
  COS = 0,
  SIN = 1,
  EX2 = 2,
  LG2 = 3,
  RCP = 4,
};
} // namespace MufuFlag

} // namespace GASS

} // namespace llvm

#endif