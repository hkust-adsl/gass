#include "GASS.h"
#include "GASSTargetMachine.h"
#include "TargetInfo/GASSTargetInfo.h"
#include "MCTargetDesc/GASSTargetStreamer.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCContext.h"

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

namespace {
// A better name may be "StreamLoweringDriver"?
// MI -> MCI -> MCStreamer
class GASSAsmPrinter : public AsmPrinter {
  const GASSSubtarget *Subtarget;
  const GASSTargetObjectFile *GTOF = nullptr;
public:
  explicit GASSAsmPrinter(TargetMachine &TM,
                          std::unique_ptr<MCStreamer> Streamer)
    : AsmPrinter(TM, std::move(Streamer)) {
    GASSTargetMachine &GTM = static_cast<GASSTargetMachine &>(TM);
    Subtarget = GTM.getSubtargetImpl();

    GTOF = static_cast<GASSTargetObjectFile*>(TM.getObjFileLowering());
  }

  StringRef getPassName() const override { return "GASS Assembly Printer"; }

  //=---------------Override emit functions----------------=//
  void emitInstruction(const MachineInstr *) override;

  //=----------------body of the asm file-------------------=//
  void emitStartOfAsmFile(Module &M) override;
  // void emitEndOfAsmFile(Module &M) override;

  // Add new sections for .nv.info.{name}, .nv.constant0.{name}
  void emitFunctionBodyEnd() override;
};
}

void GASSAsmPrinter::emitInstruction(const MachineInstr *MI) {
  MCInst Inst;
  LowerToMCInst(MI, Inst);
  EmitToStreamer(*OutStreamer, Inst);
}

// This is called by doInitialization(Module &M)
void GASSAsmPrinter::emitStartOfAsmFile(Module &M) {
  GASSTargetStreamer &GTS = 
    static_cast<GASSTargetStreamer &>(*OutStreamer->getTargetStreamer());

  unsigned SmVersion = Subtarget->getSmVersion();
  GTS.emitAttributes(SmVersion);
}

// Add cubin ELF seconds
// .nv.constant0.{name}
void GASSAsmPrinter::emitFunctionBodyEnd() {
  // TODO: do not let asm emit this?
  // .nv.constant0.{name} section
  MCSection *Constant0Section = GTOF->getConstant0NamedSection(
                                         &MF->getFunction());
  OutStreamer->SwitchSection(Constant0Section);
  OutStreamer->emitZeros(128); // TODO: change this

  // .nv.info.{name} section
  MCSection *NVInfoSection = GTOF->getNvInfoNamedSection(&MF->getFunction());
  OutStreamer->SwitchSection(NVInfoSection); // Empty


}


// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeGASSAsmPrinter() {
  RegisterAsmPrinter<GASSAsmPrinter> X(getTheGASSTarget());
}