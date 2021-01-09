#include "GASS.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/MC/MCInst.h"

using namespace llvm;

static void lowerToMCOperand(const MachineOperand &MO, MCOperand &MCOp) {
  // 
}

void llvm::LowerToMCInst(const MachineInstr *MI, MCInst &Inst) {
  Inst.setOpcode(MI->getOpcode());

  for (unsigned i=0; i != MI->getNumOperands(); ++i) {
    const MachineOperand &MO = MI->getOperand(i);
    MCOperand MCOp;
    lowerToMCOperand(MO, MCOp);
    Inst.addOperand(MCOp);
  }

  // Store control info in flags :) amazing
  Inst.setFlags(0x000fea00);
}