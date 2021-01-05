#include "GASSAsmBackend.h"
#include "GASSELFObjectWriter.h"

using namespace llvm;

std::unique_ptr<MCObjectTargetWriter>
GASSAsmBackend::createObjectTargetWriter() const {
  return std::make_unique<GASSELFObjectWriter>(OSABI);
}