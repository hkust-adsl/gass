#include "GASS.h"
#include "GASSInstPrinter.h"
#include "GASSMCTargetDesc.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/raw_ostream.h"

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
  } else if (Op.isFPImm()) {
    // APFloat(Op.getFPImm()).print(O);
    O << Op.getFPImm();
  } else {
    assert(Op.isExpr() && "unknown operand kind in printOperand");
    Op.getExpr()->print(O, &MAI);
  }
}

// Private helper functions
void GASSInstPrinter::printRegOperand(unsigned RegNo, raw_ostream &O) {
  StringRef RegName(getRegisterName(RegNo));

  O << RegName;
}


//=-------------------------------=//
// Custom print function (tablegen'erated)
void GASSInstPrinter::printConstantMem(const MCInst *MI, 
                                       unsigned OpNo, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  // example: c[0x0][0x160]
  unsigned value = Op.getImm();
  O << "c[0x0][" << format_hex(value, 3) << "]";
}

void GASSInstPrinter::printPredNot(const MCInst *MI, unsigned OpNo, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  unsigned Val = Op.getImm();
  if (Val == 0) O << "";
  else O << "!";
}

void GASSInstPrinter::printCmpMode(const MCInst *MI, 
                                   unsigned OpNo, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);

  unsigned Value = Op.getImm();
  switch (Value) {
  default:  llvm_unreachable("Invalid Cmp");
  case GASS::GASSCC::CondCode::LT: O << ".LT";  return;
  case GASS::GASSCC::CondCode::EQ: O << ".EQ";  return;
  case GASS::GASSCC::CondCode::LE: O << ".LE";  return;
  case GASS::GASSCC::CondCode::GT: O << ".GT";  return;
  case GASS::GASSCC::CondCode::NE: O << ".NE";  return;
  case GASS::GASSCC::CondCode::GE: O << ".GE";  return;
  case GASS::GASSCC::CondCode::LO: O << ".LO";  return;
  }
}

void GASSInstPrinter::printCmpModeSign(const MCInst *MI, 
                                       unsigned OpNo, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);

  unsigned Value = Op.getImm();
  switch (Value) {
  default:  llvm_unreachable("Invalid Cmp Sign");
  case GASS::GASSCC::CondCodeSign::U32: O << ".U32";  return;
  case GASS::GASSCC::CondCodeSign::S32: O << ".S32";  return;
  }
}

void GASSInstPrinter::printCmpLogic(const MCInst *MI, 
                                    unsigned OpNo, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  assert(Op.isImm());
  unsigned Value = Op.getImm();
  switch (Value) {
  default: llvm_unreachable("error");
  case GASS::GASSCC::AND: O << ".AND"; return;
  case GASS::GASSCC::XOR: O << ".XOR"; return;
  case GASS::GASSCC::OR:  O << ".OR"; return;
  }
}

void GASSInstPrinter::printShflMode(const MCInst *MI, 
                                    unsigned OpNo, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);

  unsigned Value = Op.getImm();
  switch (Value) {
  default: llvm_unreachable("Invalid shfl mode");
  case GASS::ShflMode::IDX: O << ".IDX"; return;
  case GASS::ShflMode::UP: O << ".UP"; return;
  case GASS::ShflMode::DOWN: O << ".DOWN"; return;
  case GASS::ShflMode::BFLY: O << ".BFLY"; return;
  }
}

void GASSInstPrinter::printMmaLayout(const MCInst *MI,
                                     unsigned OpNo, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);

  assert(Op.isImm());
  unsigned Value = Op.getImm();
  switch (Value) {
  default: llvm_unreachable("error");
  case GASS::TensorCore::ROW: O << ".ROW"; return;
  case GASS::TensorCore::COL: O << ".COL"; return;
  }
}

void GASSInstPrinter::printMmaStep(const MCInst *MI,
                                   unsigned OpNo, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);

  assert(Op.isImm());
  unsigned Value = Op.getImm();
  switch (Value) {
  default: llvm_unreachable("error");
  case 0: O << ".STEP0"; return;
  case 1: O << ".STEP1"; return;
  case 2: O << ".STEP2"; return;
  case 3: O << ".STEP3"; return;
  }
}

void GASSInstPrinter::printPredicateOperand(const MCInst *MI, unsigned OpNo,
                                            raw_ostream &O) {
  const MCOperand &PredFlip = MI->getOperand(OpNo);
  const MCOperand &PredMask = MI->getOperand(OpNo+1);
  assert(PredFlip.isImm() && PredMask.isReg());
  // don't print @PT
  if (PredFlip.getImm() == 0 && PredMask.getReg() == GASS::PT) 
    return;
  // don't print @UPT
  if (PredFlip.getImm() == 0 && PredMask.getReg() == GASS::UPT)
    return;
  O << "@";
  if (PredFlip.getImm() != 0)
    O << "!";
  printRegOperand(PredMask.getReg(), O);
}

void GASSInstPrinter::printShiftDir(const MCInst *MI, 
                                    unsigned OpNo, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  assert(Op.isImm());
  unsigned Value = Op.getImm();
  switch (Value) {
  default: llvm_unreachable("Invalid shift dir");
  case GASS::SHF_FLAGS::R: O << ".R"; return;
  case GASS::SHF_FLAGS::L: O << ".L"; return;
  }
}

void GASSInstPrinter::printShiftType(const MCInst *MI,
                                     unsigned OpNo, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  assert(Op.isImm());
  unsigned Value = Op.getImm();
  switch (Value) {
  default: llvm_unreachable("Invalid shift type");
  case GASS::SHF_FLAGS::U32: O << ".U32"; return;
  case GASS::SHF_FLAGS::S32: O << ".S32"; return;  
  case GASS::SHF_FLAGS::U64: O << ".U64"; return;
  case GASS::SHF_FLAGS::S64: O << ".S64"; return;
  }
}

void GASSInstPrinter::printShiftLoc(const MCInst *MI,
                                    unsigned OpNo, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  assert(Op.isImm());
  unsigned Value = Op.getImm();
  switch (Value) {
  default: llvm_unreachable("Invalid shift loc");
  case GASS::SHF_FLAGS::LO: return; // print nothing
  case GASS::SHF_FLAGS::HI: O << ".HI"; return;
  }
}

void GASSInstPrinter::printPredicateSign(const MCInst *MI,
                                         unsigned OpNo, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  assert(Op.isImm());
  unsigned Value = Op.getImm();
  switch (Value) {
  default: llvm_unreachable("Invalid");
  case 0 : return;
  case 1 : O << "!"; return;
  }
}

void GASSInstPrinter::printLdsmTrans(const MCInst *MI, unsigned OpNo, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  assert(Op.isImm());
  unsigned Value = Op.getImm();
  switch (Value) {
  default: llvm_unreachable("Invalid");
  case 0: O << "M88"; return;
  case 1: O << "MT88"; return;
  }
}

void GASSInstPrinter::printMufuFlag(const MCInst *MI, unsigned OpNo, raw_ostream &O) {
  const MCOperand &Op = MI->getOperand(OpNo);
  assert(Op.isImm());
  unsigned Flag = Op.getImm();
  switch (Flag) {
  default: llvm_unreachable("Invalid");
  case GASS::MufuFlag::COS: O << "COS"; return;
  case GASS::MufuFlag::SIN: O << "SIN"; return;
  case GASS::MufuFlag::EX2: O << "EX2"; return;
  case GASS::MufuFlag::LG2: O << "LG2"; return;
  case GASS::MufuFlag::RCP: O << "RCP"; return;
  }
}