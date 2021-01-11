#ifndef LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSMCCODEEMITTER_H
#define LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSMCCODEEMITTER_H

#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCInstrInfo.h"

namespace llvm {
class GASSMCCodeEmitter : public MCCodeEmitter {
  MCContext &Ctx;
  const MCInstrInfo &MCII;
public:
  GASSMCCodeEmitter(MCContext &ctx, const MCInstrInfo &MCII)
    : Ctx(ctx), MCII(MCII) {}

  ~GASSMCCodeEmitter() override {}

  void encodeInstruction(const MCInst &MI, raw_ostream &OS,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;
  
  // TableGen'eranted function
  void getBinaryCodeForInstr(const MCInst &MI,
                             SmallVectorImpl<MCFixup> &Fixups,
                             APInt &Inst,
                             APInt &Scratch,
                             const MCSubtargetInfo &STI) const;

  // Required by TableGen'erated functions
  void getMachineOpValue(const MCInst &MI, const MCOperand &MO,
                         APInt &Op,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const;
};
} // namespace llvm

#endif