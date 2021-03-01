#include "GASSMCTargetDesc.h"
#include "GASSMCAsmInfo.h"
#include "GASSInstPrinter.h"
#include "GASSMCCodeEmitter.h"
#include "GASSTargetStreamer.h"
#include "GASSAsmStreamer.h"
#include "GASSELFStreamer.h"
#include "GASSELFObjectWriter.h"
#include "GASSAsmBackend.h"
#include "TargetInfo/GASSTargetInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/Support/TargetRegistry.h"

using namespace llvm;

#define GET_INSTRINFO_MC_DESC
#include "GASSGenInstrInfo.inc"

#define GET_REGINFO_MC_DESC
#include "GASSGenRegisterInfo.inc"

#define GET_SUBTARGETINFO_MC_DESC
#include "GASSGenSubtargetInfo.inc"

static MCRegisterInfo *createGASSMCRegisterInfo(const Triple &TT) {
  MCRegisterInfo *X = new MCRegisterInfo();
  InitGASSMCRegisterInfo(X, /*TODO*/0); // table-gen'd
  return X;
}

static MCInstrInfo *createGASSMCInstrInfo() {
  MCInstrInfo *X = new MCInstrInfo();
  InitGASSMCInstrInfo(X);
  return X;
}

static MCSubtargetInfo *createGASSMCSubtargetInfo(const Triple &TT,
                                                  StringRef CPU, StringRef FS) {
  return createGASSMCSubtargetInfoImpl(TT, CPU, CPU, FS);
}

static MCAsmInfo *createGASSMCAsmInfo(const MCRegisterInfo &MRI,
                                      const Triple &TT,
                                      const MCTargetOptions &Options) {
  MCAsmInfo *MAI = new GASSMCAsmInfo(TT, Options);

  return MAI;
}

static MCInstPrinter *createGASSMCInstPrinter(const Triple &T,
                                              unsigned SyntaxVariant,
                                              const MCAsmInfo &MAI,
                                              const MCInstrInfo &MII,
                                              const MCRegisterInfo &MRI) {
  return new GASSInstPrinter(MAI, MII, MRI);
}

static MCCodeEmitter *createGASSMCCodeEmitter(const MCInstrInfo &MCII,
                                              const MCRegisterInfo &MRI,
                                              MCContext &CTX) {
  return new GASSMCCodeEmitter(CTX, MCII);
}

static MCAsmBackend *createGASSAsmBackend(const Target &T, 
                                          const MCSubtargetInfo &STI,
                                          const MCRegisterInfo &MRI,
                                          const MCTargetOptions &Options) {
  const Triple &TT = STI.getTargetTriple();
  uint8_t OSABI = MCELFObjectTargetWriter::getOSABI(TT.getOS());
  return new GASSAsmBackend(OSABI);
}

//=------------------------------------------------------=//
// Emit file (MCStreamer/AsmPrinter)
//=------------------------------------------------------=//
static MCTargetStreamer *
createGASSObjectTargetStreamer(MCStreamer &S, const MCSubtargetInfo &STI) {
  return new GASSTargetELFStreamer(S, STI);
}

static MCTargetStreamer *createGASSAsmTargetStreamer(MCStreamer &S, 
                                                     formatted_raw_ostream &OS,
                                                     MCInstPrinter *InstPrint,
                                                     bool isVerboseAsm) {
  return new GASSTargetAsmStreamer(S, OS);
}



extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeGASSTargetMC() {
  Target &T = getTheGASSTarget();
  // Required by GASSTargetMachine.initAsmInfo()
  TargetRegistry::RegisterMCRegInfo(T, createGASSMCRegisterInfo);
  TargetRegistry::RegisterMCInstrInfo(T, createGASSMCInstrInfo);
  TargetRegistry::RegisterMCSubtargetInfo(T, createGASSMCSubtargetInfo);
  TargetRegistry::RegisterMCAsmInfo(T, createGASSMCAsmInfo);

  // InstPrinter?
  TargetRegistry::RegisterMCInstPrinter(T, createGASSMCInstPrinter);
  TargetRegistry::RegisterMCCodeEmitter(T, createGASSMCCodeEmitter);

  // // Register the obj streamer.
  // TargetRegistry::RegisterELFStreamer(T, createELFStreamer);

  // Register ELFWriter?

  TargetRegistry::RegisterMCAsmBackend(T, createGASSAsmBackend);
  // Emit file (MCStreamer/AsmPrinter)
  TargetRegistry::RegisterObjectTargetStreamer(T, 
                                               createGASSObjectTargetStreamer);
  TargetRegistry::RegisterAsmTargetStreamer(T, createGASSAsmTargetStreamer);
}