#include "GASS.h"
#include "GASSSubtarget.h"
#include "GASSTargetMachine.h"
#include "GASSMCInstLowering.h"
#include "TargetInfo/GASSTargetInfo.h"
#include "MCTargetDesc/GASSTargetStreamer.h"
#include "MCTargetDesc/NvInfo.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSymbolELF.h"

#include <vector>
#include <memory>

using namespace llvm;

#define DEBUG_TYPE "asm-printer"

namespace {
/// Helper to store argument info
struct GASSArgument {
  unsigned Offset;
  unsigned Size;
  unsigned Ordi;
  unsigned LogAlignment;
  GASSArgument(unsigned Offset, unsigned Size, 
               unsigned Ordi, unsigned LogAlignment)
    : Offset(Offset), Size(Size), Ordi(Ordi), LogAlignment(LogAlignment) {}
};

// A better name may be "StreamLoweringDriver"?
// MI -> MCI -> MCStreamer
class GASSAsmPrinter : public AsmPrinter {
  GASSMCInstLower MCInstLowering;
  const GASSTargetMachine *GTM = nullptr;
  const GASSSubtarget *Subtarget = nullptr;
  const GASSTargetObjectFile *GTOF = nullptr;

  /// Record offsets of EXITs (nv.info needs them)
  std::vector<unsigned> EXITOffsets;
  /// Current Instr Offset (in bytes)
  unsigned CurrentOffset; 

  /// Module-level .nv.info
  std::vector<std::unique_ptr<NvInfo>> ModuleInfo;

public:
  explicit GASSAsmPrinter(TargetMachine &TM,
                          std::unique_ptr<MCStreamer> Streamer)
    : AsmPrinter(TM, std::move(Streamer)), MCInstLowering(OutContext, *this) {
    GTM = static_cast<GASSTargetMachine *>(&TM);
    Subtarget = GTM->getSubtargetImpl();

    GTOF = static_cast<GASSTargetObjectFile*>(TM.getObjFileLowering());
  }

  StringRef getPassName() const override { return "GASS Assembly Printer"; }

  //=---------------Override emit functions----------------=//
  void emitInstruction(const MachineInstr *) override;

  //=----------------body of the asm file-------------------=//
  void emitStartOfAsmFile(Module &M) override;
  // void emitEndOfAsmFile(Module &M) override;

  // record MF order
  void emitFunctionBodyStart() override;

  // Add new sections for .nv.info.{name}, .nv.constant0.{name}
  void emitFunctionBodyEnd() override;

  // Symtab
  void emitEndOfAsmFile(Module &M) override;

  GASSTargetStreamer* getTargetStreamer() const;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineModuleInfoWrapperPass>();
    AU.addPreserved<MachineModuleInfoWrapperPass>();
    AsmPrinter::getAnalysisUsage(AU);
  }

private:
  void generateNvInfoModule(MachineFunction &MF);

  /// @return constant0 size in byte
  unsigned generateNvInfo(MachineFunction &MF,
                          std::vector<std::unique_ptr<NvInfo>> &Info);
};
} // anonymous namespace

// helper
// generate .nv.info (per module)
void GASSAsmPrinter::generateNvInfoModule(MachineFunction &MF) {
  // REGCOUNT
  // TODO: get this value from register allocator
  ModuleInfo.emplace_back(createNvInfoRegCount(/*Count*/8, &MF));
  // MAX_STACK_SIZE
  // TODO: get this value from register allocator
  ModuleInfo.emplace_back(createNvInfoMaxStackSize(/*MaxSize*/0, &MF));
  // MIN_STACK_SIZE
  ModuleInfo.emplace_back(createNvInfoMinStackSize(0, &MF));
  // FRAME_SIZE
  ModuleInfo.emplace_back(createNvInfoFrameSize(0, &MF));
}

// static helpers
static unsigned calLog2(unsigned I) {
  return (I > 1) ? 1 + calLog2(I >> 1) : 0;
}

static unsigned alignTo(unsigned CurOff, unsigned AlignReq) {
  while (CurOff % AlignReq != 0)
    CurOff++;

  return CurOff;    
}

unsigned GASSAsmPrinter::generateNvInfo(MachineFunction &MF,
                                  std::vector<std::unique_ptr<NvInfo>> &Info) {
  const GASSSubtarget &Subtarget = 
    *static_cast<const GASSSubtarget*>(&MF.getSubtarget());

  Info.emplace_back(createNvInfoSwWar(true));
  // TODO: query Subtarget
  Info.emplace_back(createNvInfoCudaVersion(100));

  unsigned ParamBaseOffset = Subtarget.getParamBase();
  // Note: this is different from LowerFormalArguements()
  unsigned CurrentParamOffset = 0;
  
  std::vector<GASSArgument> Arguments;
  const DataLayout &DL = MF.getDataLayout();
  // collect arg info
  for (const Argument &I : MF.getFunction().args()) {
    Type *Ty = I.getType();
    assert(Ty->getScalarSizeInBits() % 8 == 0 && 
           "Sub-byte argument not supported yet, to be extended");
    unsigned SizeInBytes;
    if (Ty->isPointerTy()) {
      SizeInBytes = DL.getPointerTypeSize(Ty);
    } else {
      SizeInBytes = Ty->getScalarSizeInBits() / 8;
    }
    unsigned AlignRequirement = DL.getABITypeAlignment(Ty);
    CurrentParamOffset = alignTo(CurrentParamOffset, AlignRequirement);

    Arguments.emplace_back(CurrentParamOffset, SizeInBytes, 0, 
                           AlignRequirement);

    CurrentParamOffset += SizeInBytes;
  }

  Info.emplace_back(createNvInfoParamCBank(ParamBaseOffset, 
                                           CurrentParamOffset, &MF));
  Info.emplace_back(createNvInfoCBankParamSize(CurrentParamOffset));
  // for each arguments
  for (unsigned i=0; i<Arguments.size(); ++i) {
    GASSArgument &Arg = Arguments[i];
    Info.emplace_back(createNvInfoKParamInfo(0, i, 
                                             Arg.Offset, Arg.Size, 
                                             calLog2(Arg.LogAlignment)));
  }
  Info.emplace_back(createNvInfoMaxRegCount(255));
  Info.emplace_back(createNvInfoExitInstrOffsets({EXITOffsets}));

  return ParamBaseOffset + CurrentParamOffset;
}

GASSTargetStreamer* GASSAsmPrinter::getTargetStreamer() const {
  if (!OutStreamer)
    return nullptr;
  return static_cast<GASSTargetStreamer*>(OutStreamer->getTargetStreamer());
}

void GASSAsmPrinter::emitInstruction(const MachineInstr *MI) {
  MCInst Inst;
  // Record EXITs' offset (.nv.info needs them)
  if (MI->getOpcode() == GASS::EXIT)
    EXITOffsets.push_back(CurrentOffset);

  MCInstLowering.LowerToMCInst(MI, Inst);
  EmitToStreamer(*OutStreamer, Inst);

  // FIXME: Should be architecture-dependent. i.e., query MCInstrDesc
  CurrentOffset += 16;
}

// This is called by doInitialization(Module &M)
void GASSAsmPrinter::emitStartOfAsmFile(Module &M) {
  unsigned SmVersion = Subtarget->getSmVersion();
  getTargetStreamer()->emitAttributes(SmVersion);

  // Record num of MFs (to predicate symtab idx)
  getTargetStreamer()->recordNumMFs(M.size());
}

/// Cache MF visiting order
void GASSAsmPrinter::emitFunctionBodyStart() {
  getTargetStreamer()->visitMachineFunction(MF);
}

// Add cubin ELF seconds
// .nv.constant0.{name}
void GASSAsmPrinter::emitFunctionBodyEnd() {
  // .nv.info.{name} section
  MCSection *NVInfoSection = GTOF->getNvInfoNamedSection(&MF->getFunction());
  OutStreamer->SwitchSection(NVInfoSection); // Empty
  // TODO: complete this
  std::vector<std::unique_ptr<NvInfo>> FuncInfo;
  unsigned Constant0Size = generateNvInfo(*MF, FuncInfo);
  getTargetStreamer()->emitNvInfoFunc(FuncInfo);

  // .nv.constant0.{name} section
  MCSection *Constant0Section = GTOF->getConstant0NamedSection(
                                         &MF->getFunction());
  OutStreamer->SwitchSection(Constant0Section);
  OutStreamer->emitZeros(Constant0Size);

  // Generate module-level .nv.info (not emit)
  generateNvInfoModule(*MF);

  // reset per-function data
  EXITOffsets.clear();
  CurrentOffset = 0;
}

// symtab
void GASSAsmPrinter::emitEndOfAsmFile(Module &M) {
  // emit module-level .nv.info
  MCSection *ModuleNvInfoSection = GTOF->getNvInfoSection();
  OutStreamer->SwitchSection(ModuleNvInfoSection);
  getTargetStreamer()->emitNvInfo(ModuleInfo);

  for (const Function &F : M) {
    // TODO: change this to .nv.info.{name}
    MCSymbol *Name = GTM->getSymbol(&F);
    OutStreamer->emitSymbolAttribute(Name, MCSA_Global);
  }
}


// Force static initialization.
extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeGASSAsmPrinter() {
  RegisterAsmPrinter<GASSAsmPrinter> X(getTheGASSTarget());
}