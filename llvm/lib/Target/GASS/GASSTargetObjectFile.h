#ifndef LLVM_LIB_TARGET_GASS_GASSTARGETOBJECTFILE_H
#define LLVM_LIB_TARGET_GASS_GASSTARGETOBJECTFILE_H

#include "llvm/MC/MCSection.h"
#include "llvm/MC/SectionKind.h"
#include "llvm/Target/TargetLoweringObjectFile.h"

namespace llvm {

// This seems like a layering problem.
// Maybe we should move this to "MCTargetDesc"?
// GASSTargetObjectFile : public TargetLoweringObjectFile 
//                      : public MCObjectFileInfo
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

  // section that contains encoded code
  // .text.{name}
  MCSection *SelectSectionForGlobal(const GlobalObject *GO, SectionKind Kind,
                                    const TargetMachine &TM) const override;

  //=--------GASS specific sections-------------------=//
  MCSection *getConstant0NamedSection(const Function *F) const;
  MCSection *getNvInfoNamedSection(const Function *F) const;
};

} // namespace llvm

#endif