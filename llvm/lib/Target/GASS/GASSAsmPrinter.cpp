#include "GASS.h"
#include "TargetInfo/GASSTargetInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/MC/MCStreamer.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

namespace {
// A better name may be "StreamLoweringDriver"?
// MI -> MCI -> MCStreamer
class GASSAsmPrinter : public AsmPrinter {
public:
  explicit GASSAsmPrinter(TargetMachine &TM,
                          std::unique_ptr<MCStreamer> Streamer)
    : AsmPrinter(TM, std::move(Streamer)) {}

  StringRef getPassName() const override { return "GASS Assembly Printer"; }

  //=---------------Override emit functions----------------=//
  void emitInstruction(const MachineInstr *) override;

  //=----------------body of the asm file-------------------=//
  void emitStartOfAsmFile(Module &M) override;
  void emitEndOfAsmFile(Module &M) override;
};
}

void GASSAsmPrinter::emitInstruction(const MachineInstr *MI) {
  MCInst Inst;
  LowerToMCInst(MI, Inst);
  EmitToStreamer(*OutStreamer, Inst);
}

void GASSAsmPrinter::emitStartOfAsmFile(Module &M) {
  // 
}

void GASSAsmPrinter::emitEndOfAsmFile(Module &M) {
  //
}

// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeGASSAsmPrinter() {
  RegisterAsmPrinter<GASSAsmPrinter> X(getTheGASSTarget());
}