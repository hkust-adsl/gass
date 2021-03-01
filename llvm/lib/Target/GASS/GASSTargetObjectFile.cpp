#include "GASSTargetObjectFile.h"
#include "llvm/IR/Function.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"

using namespace llvm;

// GASS specific ELF INFO
#define CUDA_INFO 0x70000000
 
MCSection * GASSTargetObjectFile::SelectSectionForGlobal(
                                    const GlobalObject *GO, SectionKind Kind,
                                    const TargetMachine &TM) const {
  // GO (GlobalObject->Function)
  StringRef Name;
  if (auto *F = dyn_cast<Function>(GO)) {
    Name = F->getName();
  }

  // TODO: add Link & Info & symtab
  return getContext().getELFSection(".text." + Name, 
                                    /*Type*/ELF::SHT_PROGBITS,
                                    /*Flags*/ELF::SHF_ALLOC | 
                                            ELF::SHF_EXECINSTR);
}

// .nv.constant0.{name} sections
MCSection* 
GASSTargetObjectFile::getConstant0NamedSection(const Function *F) const {
  StringRef Name = F->getName();

  return getContext().getELFSection(".nv.constant0." + Name, 
                                    /*Type*/ELF::SHT_PROGBITS,
                                    /*Flags*/ELF::SHF_ALLOC);
}

// .nv.info.{name} sections
MCSection* 
GASSTargetObjectFile::getNvInfoNamedSection(const Function *F) const {
  StringRef Name = F->getName();

  return getContext().getELFSection(".nv.info." + Name, 
                                    /*Type*/CUDA_INFO,
                                    /*Flags*/0);
}

// .nv.info section
MCSection*
GASSTargetObjectFile::getNvInfoSection() const {
  return getContext().getELFSection(".nv.info",
                                    /*Type*/CUDA_INFO,
                                    /*Flags*/0);
}