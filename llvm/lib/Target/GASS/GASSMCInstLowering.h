#ifndef LLVM_LIB_TARGET_GASS_GASSMCINSTLOWERING_H
#define LLVM_LIB_TARGET_GASS_GASSMCINSTLOWERING_H

namespace llvm {
class AsmPrinter;
class MCAsmInfo;
class MCContext;
class MCInst;
class MCOperand;
class MCSymbol;
class MachineInstr;
class MachineModuleInfoMachO;
class MachineOperand;
class Mangler;

class GASSMCInstLower {
  MCContext &Ctx;
  AsmPrinter &Printer;

public:
  GASSMCInstLower(MCContext &ctx, AsmPrinter &printer)
    : Ctx(ctx), Printer(printer) {}
  void LowerToMCInst(const MachineInstr *MI, MCInst &Inst);

private:
  void lowerToMCOperand(const MachineOperand &MO, MCOperand &MCOp);
  void lowerToMCFlags(const MachineInstr &MI, MCInst &MCI);
};
} // namespace llvm

#endif