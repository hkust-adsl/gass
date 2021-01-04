#ifndef LLVM_LIB_TARGET_GASS_GASSREGISTERBANKINFO_H
#define LLVM_LIB_TARGET_GASS_GASSREGISTERBANKINFO_H

#include "llvm/CodeGen/GlobalISel/RegisterBankInfo.h"

#define GET_REGBANK_DECLARATIONS
#include "GASSGenRegisterBank.inc"

namespace llvm {
class TargetRegisterInfo;

class GASSGenRegisterBankInfo : public RegisterBankInfo {
protected:
#define GET_TARGET_REGBANK_CLASS
#include "GASSGenRegisterBank.inc"
};

class GASSRegisterBankInfo final : public GASSGenRegisterBankInfo {
public:
  GASSRegisterBankInfo(const TargetRegisterInfo &TRI);
};

} // namespace llvm

#endif