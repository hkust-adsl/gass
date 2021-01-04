#ifndef LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSMCASMINFO_H
#define LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSMCASMINFO_H

#include "llvm/MC/MCAsmInfo.h"

namespace llvm {
class Triple;

class GASSMCAsmInfo : public MCAsmInfo {
  virtual void anchor() {} // what's this for?

public:
  explicit GASSMCAsmInfo(const Triple &TT,
                       const MCTargetOptions &Options);
  
  bool shouldOmitSectionDirective(StringRef SectionName) const override {
    return true;
  }
};
} // namespace llvm

#endif