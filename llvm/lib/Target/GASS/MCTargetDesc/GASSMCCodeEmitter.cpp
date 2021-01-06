#include "GASSMCTargetDesc.h"
#include "GASSMCCodeEmitter.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// getBinaryCodeForInstr
#include "GASSGenMCCodeEmitter.inc"

void GASSMCCodeEmitter::encodeInstruction(const MCInst &MI, raw_ostream &OS,
                                          SmallVectorImpl<MCFixup> &Fixups,
                                          const MCSubtargetInfo &STI) const {
  APInt Inst(128, 0);
  getBinaryCodeForInstr(MI, Fixups, Inst, /*Scratch*/Inst, STI);
  support::endian::write(OS, Inst.getRawData()[0], support::little);
  support::endian::write(OS, Inst.getRawData()[1], support::little);
}