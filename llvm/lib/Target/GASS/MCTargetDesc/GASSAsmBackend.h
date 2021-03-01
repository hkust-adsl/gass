#ifndef LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSASMBACKEND_H
#define LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSASMBACKEND_H

#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCObjectWriter.h"

namespace llvm {
class GASSAsmBackend : public MCAsmBackend {
  uint8_t OSABI;
public:
  GASSAsmBackend(uint8_t OSABI)
    : MCAsmBackend(support::little),
      OSABI(OSABI) {}

  //=-------Override pure virtual functions---------=//
  std::unique_ptr<MCObjectTargetWriter> 
  createObjectTargetWriter() const override;

  unsigned getNumFixupKinds() const override {
    return 0;
  }

  void applyFixup(const MCAssembler &Asm, const MCFixup &Fixup,
                  const MCValue &Target, MutableArrayRef<char> Data,
                  uint64_t Value, bool IsResolved,
                  const MCSubtargetInfo *STI) const override {}

  bool fixupNeedsRelaxation(const MCFixup &Fixup, uint64_t Value,
                            const MCRelaxableFragment *DF,
                            const MCAsmLayout &Layout) const override {
    // TODO: fill this
    return false;
  }

  bool writeNopData(raw_ostream &OS, uint64_t Count) const override {
    return true;
  }

  std::unique_ptr<MCObjectWriter>
  createObjectWriter(raw_pwrite_stream &OS) const override;
};

// createObjectWriter
std::unique_ptr<MCObjectWriter>
createCubinObjectWriter(std::unique_ptr<MCELFObjectTargetWriter> MOTW,
                        raw_pwrite_stream &OS, bool ISLittleEndian);
}

#endif