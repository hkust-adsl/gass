#include "GASSMCTargetDesc.h"
#include "GASSMCCodeEmitter.h"

using namespace llvm;

// getBinaryCodeForInstr
#include "GASSGenMCCodeEmitter.inc"

void GASSMCCodeEmitter::encodeInstruction(const MCInst &MI, raw_ostream &OS,
                                          SmallVectorImpl<MCFixup> &Fixups,
                                          const MCSubtargetInfo &STI) const {
  // getBinaryCodeForInstr();
  // support::endian::write(OS, Bits, support::little);
}