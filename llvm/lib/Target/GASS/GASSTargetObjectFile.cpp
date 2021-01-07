#include "GASSTargetObjectFile.h"
#include "llvm/IR/Function.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionELF.h"

using namespace llvm;
 
MCSection * GASSTargetObjectFile::SelectSectionForGlobal(
                                    const GlobalObject *GO, SectionKind Kind,
                                    const TargetMachine &TM) const {
  // GO (GlobalObject->Function)
  StringRef Name;
  if (auto *F = dyn_cast<Function>(GO)) {
    Name = F->getName();
  }

  // TODO: add Link & Info & symtab
  // TODO: should be use this?
  return getContext().getELFNamedSection(".text", Name, ELF::SHT_PROGBITS,
                                         ELF::SHF_WRITE | ELF::SHF_ALLOC);
}