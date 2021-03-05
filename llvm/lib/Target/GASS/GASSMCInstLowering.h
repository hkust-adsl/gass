#ifndef LLVM_LIB_TARGET_GASS_GASSMCINSTLOWERING_H
#define LLVM_LIB_TARGET_GASS_GASSMCINSTLOWERING_H

namespace llvm {
class AsmPrinter;
class MCAsmInfo;
class MCContext;
class MCInst;
class MachineBasicBlock;
class MCOperand;
class MCSymbol;
class MachineInstr;
class MachineModuleInfoMachO;
class MachineOperand;
class Mangler;

class GASSMCInstLower {
  MCContext &Ctx;
  AsmPrinter &Printer;

  /// Cache MI & MBB offsets to compute BrOffset
  /// Ref: BranchReleaxation
  DenseMap<const MachineInstr *, uint64_t> *MIOffsets;
  DenseMap<const MachineBasicBlock *, uint64_t> *MBBOffsets;

public:
  GASSMCInstLower(MCContext &ctx, AsmPrinter &printer)
    : Ctx(ctx), Printer(printer) {}
  void LowerToMCInst(const MachineInstr *MI, MCInst &Inst,
                     DenseMap<const MachineBasicBlock*, uint64_t> MBBOffsets,
                     DenseMap<const MachineInstr *, uint64_t> MIOffsets);

private:
  void lowerToMCOperand(const MachineOperand &MO, MCOperand &MCOp);
  void lowerToMCFlags(const MachineInstr &MI, MCInst &MCI);
};
} // namespace llvm

#endif