#include "GASSInstPrinter.h"
#include "llvm/MC/MCInst.h"
#include "llvm/Support/FormattedStream.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

// #define PRINT_ALIAS_INSTR
#include "GASSGenAsmWriter.inc"

GASSInstPrinter::GASSInstPrinter(const MCAsmInfo &MAI, const MCInstrInfo &MII,
                                 const MCRegisterInfo &MRI)
  : MCInstPrinter(MAI, MII, MRI) {}

void GASSInstPrinter::printRegName(raw_ostream &OS, unsigned RegNo) const {
  // TODO
}

void GASSInstPrinter::printInst(const MCInst *MI, uint64_t Address,
                                StringRef Annot, const MCSubtargetInfo &STI,
                                raw_ostream &OS) {
  // Auto-generated
  printInstruction(MI, Address, OS);
  printAnnotation(OS, Annot); // What's this?
}

void GASSInstPrinter::printOperand(const MCInst *MI, 
                                   unsigned OpNo, raw_ostream &O) {
  // TODO
}