#ifndef LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSELFOBJECTWRITER_H
#define LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSELFOBJECTWRITER_H

#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCELFObjectWriter.h"

namespace llvm {
class GASSELFObjectWriter : public MCELFObjectTargetWriter {
public:
  GASSELFObjectWriter(uint8_t OSABI)
    : MCELFObjectTargetWriter(/*Is64Bit*/true, 
                              /*OSABI*/OSABI,
                              /*EMachine*/ELF::EM_CUDA,
                              /*HasRelocationAddend*/false) {}

  ~GASSELFObjectWriter() override {}

  //=----------Override pure virtual function------------=//
protected:
  unsigned getRelocType(MCContext &Ctx, const MCValue &Target,
                        const MCFixup &Fixup, bool IsPCRel) const override {
    return 0; // TODO: fill this.
  }
};
}

#endif