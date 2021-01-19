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
  // This will not be called?
  OS << RegNo;
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
  const MCOperand &Op = MI->getOperand(OpNo);
  if (Op.isReg()) {
    printRegOperand(Op.getReg(), O);
  } else if (Op.isImm()) {
    O << formatImm(Op.getImm());
  } else {
    O << "/*INV_OP*/";
  }
}

// Private helper functions
void GASSInstPrinter::printRegOperand(unsigned RegNo, raw_ostream &O) {
  StringRef RegName(getRegisterName(RegNo));

  O << RegName;
}

void GASSInstPrinter::printConstantMem(const MCInst *MI, 
                                       unsigned OpNo, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  // example: c[0x0][0x160]
  unsigned value = Op.getImm();
  O << "c[0x0][" << format_hex(value, 3) << "]";
}