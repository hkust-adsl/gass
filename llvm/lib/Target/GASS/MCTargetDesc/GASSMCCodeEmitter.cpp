#include "GASS.h"
#include "GASSMCTargetDesc.h"
#include "GASSMCCodeEmitter.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/ErrorHandling.h"
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
  } else 
    llvm_unreachable("Unhandled MachineOperand type");

  return;    
}

void GASSMCCodeEmitter::encodeCmpMode(const MCInst &MI, unsigned int OpIdx, 
                                      APInt &Op, 
                                      SmallVectorImpl<MCFixup> &Fixups, 
                                      const MCSubtargetInfo &STI) const {
  APInt Enc(3, 0);
  
  const MCOperand &MO = MI.getOperand(OpIdx);

  assert(MO.isImm());

  switch (MO.getImm()) {
  default: llvm_unreachable("Invalid cmp mode");
  case GASS::GASSCC::EQ: 
    Enc = 2;
    break;
  case GASS::GASSCC::NE:
    Enc = 5;
    break;
  case GASS::GASSCC::LT:
    Enc = 1;
    break;
  case GASS::GASSCC::LE:
    Enc = 3;
    break;
  case GASS::GASSCC::GT:
    Enc = 4;
    break;
  case GASS::GASSCC::GE:
    Enc = 6;
    break;
  // TODO: EQU, NEU, ...
  }

  Op = Enc;
}

void GASSMCCodeEmitter::encodeShiftDir(const MCInst &MI, unsigned int OpIdx, 
                                       APInt &Op, 
                                       SmallVectorImpl<MCFixup> &Fixups, 
                                       const MCSubtargetInfo &STI) const {
  APInt Enc(1, 0);
  const MCOperand &MO = MI.getOperand(OpIdx);

  assert(MO.isImm());

  switch (MO.getImm()) {
  default: llvm_unreachable("Invalid flag");
  case GASS::SHF_FLAGS::L:
    Enc = 0;
    break;
  case GASS::SHF_FLAGS::R:
    Enc = 1;
    break;
  }

  Op = Enc;
}

void GASSMCCodeEmitter::encodeShiftType(const MCInst &MI, unsigned int OpIdx, 
                                        APInt &Op, 
                                        SmallVectorImpl<MCFixup> &Fixups, 
                                        const MCSubtargetInfo &STI) const {
  APInt Enc(3, 0);
  const MCOperand &MO = MI.getOperand(OpIdx);

  assert(MO.isImm());

  switch (MO.getImm()) {
  default: llvm_unreachable("Invalid flag");
  case GASS::SHF_FLAGS::U64:
  case GASS::SHF_FLAGS::S64:
    Enc = 0;
    break;
  case GASS::SHF_FLAGS::S32:
    Enc = 2;
    break;
  case GASS::SHF_FLAGS::U32:
    Enc = 3;
    break;
  }

  Op = Enc;
}

void GASSMCCodeEmitter::encodeShiftLoc(const MCInst &MI, unsigned int OpIdx, 
                                       APInt &Op, 
                                       SmallVectorImpl<MCFixup> &Fixups, 
                                       const MCSubtargetInfo &STI) const {
  APInt Enc(1, 0);
  const MCOperand &MO = MI.getOperand(OpIdx);

  assert(MO.isImm());

  switch (MO.getImm()) {
  default: llvm_unreachable("Invalid flag");
  case GASS::SHF_FLAGS::LO:
    Enc = 0;
    break;
  case GASS::SHF_FLAGS::HI:
    Enc = 1;
    break;
  }

  Op = Enc;
}