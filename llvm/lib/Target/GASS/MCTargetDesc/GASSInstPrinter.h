#ifndef LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSINSTPRINTER_H
#define LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSINSTPRINTER_H

#include "llvm/MC/MCInstPrinter.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {

class MCSubtargetInfo;

class GASSInstPrinter : public MCInstPrinter {
public:
  GASSInstPrinter(const MCAsmInfo &MAI, const MCInstrInfo &MII,
                  const MCRegisterInfo &MRI);
  
  void printRegName(raw_ostream &OS, unsigned RegNo) const override;
  void printInst(const MCInst *MI, uint64_t Address, StringRef Annot,
                 const MCSubtargetInfo &STI, raw_ostream &OS) override;
  
  // Autogenerated
  void printInstruction(const MCInst *MI, uint64_t Address, raw_ostream &O);
  static const char *getRegisterName(unsigned RegNo);
  std::pair<const char *, uint64_t> getMnemonic(const MCInst *MI) override;
  bool printAliasInstr(const MCInst *MI, uint64_t Address,
                       const MCSubtargetInfo &STI, raw_ostream &O);
  // End

  // Required by Autogenerated 
  void printOperand(const MCInst *MI, unsigned OpNo, raw_ostream &O);  

private:
  // Customized printing methods
  void printRegOperand(unsigned RegNo, raw_ostream &O);
  void printConstantMem(const MCInst *MI, unsigned OpNo, raw_ostream &O);
  void printCmpMode(const MCInst *MI, unsigned OpNo, raw_ostream &O);
  void printCmpModeSign(const MCInst *MI, unsigned OpNo, raw_ostream &O);
  void printShflMode(const MCInst *MI, unsigned OpNo, raw_ostream &O);
  void printPredicateOperand(const MCInst *MI, unsigned OpNo, raw_ostream &O);
  void printPredicateSign(const MCInst *MI, unsigned OpNo, raw_ostream &O);
  void printMmaLayout(const MCInst *MI, unsigned OpNo, raw_ostream &O);
  void printMmaStep(const MCInst *MI, unsigned OpNo, raw_ostream &O);

  // funnel shift (shf)
  void printShiftDir(const MCInst *MI, unsigned OpNo, raw_ostream &O);
  void printShiftType(const MCInst *MI, unsigned OpNo, raw_ostream &O);
  void printShiftLoc(const MCInst *MI, unsigned OpNo, raw_ostream &O);
};
}

#endif