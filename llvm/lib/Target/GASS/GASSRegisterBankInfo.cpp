#include "GASSRegisterBankInfo.h"
#include "MCTargetDesc/GASSMCTargetDesc.h"
#include "llvm/CodeGen/GlobalISel/RegisterBank.h"
#include "llvm/CodeGen/GlobalISel/RegisterBankInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_TARGET_REGBANK_IMPL
#include "GASSGenRegisterBank.inc"

using namespace llvm;

GASSRegisterBankInfo::GASSRegisterBankInfo(const TargetRegisterInfo &TRI)
  : GASSGenRegisterBankInfo() {}