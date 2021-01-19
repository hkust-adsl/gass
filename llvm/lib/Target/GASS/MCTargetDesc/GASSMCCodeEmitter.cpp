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
  APInt Scratch(128, 0); // for op
  getBinaryCodeForInstr(MI, Fixups, Inst, Scratch, STI);
  // Add control info
  uint64_t enc0 = Inst.getRawData()[1];
  uint64_t enc1 = Inst.getRawData()[0];
  enc1 |= uint64_t(MI.getFlags()) << 32;

  support::endian::write(OS, enc0, support::little);
  support::endian::write(OS, enc1, support::little);
}

void 
GASSMCCodeEmitter::getMachineOpValue(const MCInst &MI, const MCOperand &MO, 
                                     APInt &Op,
                                     SmallVectorImpl<MCFixup> &Fixups,
                                     const MCSubtargetInfo &STI) const {
  if (MO.isReg())
    Op = Ctx.getRegisterInfo()->getEncodingValue(MO.getReg());
  else if (MO.isImm()) { 
    Op = MO.getImm();
  }
  else 
    llvm_unreachable("Unhandled expression");

  return;    
}