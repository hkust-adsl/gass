#ifndef LLVM_LIB_TARGET_GASS_GASSTARGETOBJECTFILE_H
#define LLVM_LIB_TARGET_GASS_GASSTARGETOBJECTFILE_H

#include "llvm/MC/MCSection.h"
#include "llvm/MC/SectionKind.h"
#include "llvm/Target/TargetLoweringObjectFile.h"

namespace llvm {

class GASSTargetObjectFile : public TargetLoweringObjectFile {
public:
  GASSTargetObjectFile() : TargetLoweringObjectFile() {}

  void Initialize(MCContext &ctx, const TargetMachine &TM) override {
    TargetLoweringObjectFile::Initialize(ctx, TM);
  }

  MCSection *getSectionForConstant(const DataLayout &DL, SectionKind kind,
                                   const Constant *C,
                                   Align &Alignment) const override {
    return ReadOnlySection;
  }

  MCSection *getExplicitSectionGlobal(const GlobalObject *GO, SectionKind Kind,
                                      const TargetMachine &TM) const override {
    return DataSection;
  }

  MCSection *SelectSectionForGlobal(const GlobalObject *GO, SectionKind Kind,
                                    const TargetMachine &TM) const override {
    return getDataSection();
  }
};

} // namespace llvm

#endif