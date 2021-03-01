#include "GASSAsmBackend.h"
#include "GASSELFObjectWriter.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCELFObjectWriter.h"

using namespace llvm;

std::unique_ptr<MCObjectTargetWriter>
GASSAsmBackend::createObjectTargetWriter() const {
  return std::make_unique<GASSELFObjectWriter>(OSABI);
}

std::unique_ptr<MCObjectWriter>
GASSAsmBackend::createObjectWriter(raw_pwrite_stream &OS) const {
  auto TW = createObjectTargetWriter();
  return createCubinObjectWriter(cast<MCELFObjectTargetWriter>(std::move(TW)), 
                                 OS, Endian == support::little);
}