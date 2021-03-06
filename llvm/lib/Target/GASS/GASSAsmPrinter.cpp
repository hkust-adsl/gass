#include "GASS.h"
#include "GASSSubtarget.h"
#include "GASSTargetMachine.h"
#include "GASSMCInstLowering.h"
#include "GASSStallSettingPass.h"
#include "GASSBranchOffsetPass.h"
#include "TargetInfo/GASSTargetInfo.h"
#include "MCTargetDesc/GASSTargetStreamer.h"
#include "MCTargetDesc/NvInfo.h"
#include "MCTargetDesc/GASSMCTargetDesc.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/MC/MCInstBuilder.h"
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

  // Required passes
  const GASSStallSetting *StallSetter = nullptr;
  const GASSBranchOffset *BranchOffset = nullptr;

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

  bool runOnMachineFunction(MachineFunction &MF) override;

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

  // Tail & padding instructions
  void emitTailingInstructions();

  GASSTargetStreamer* getTargetStreamer() const;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    // Can not change order here.....!!!!! (OMG...)
    AU.addRequired<GASSStallSetting>();
    AsmPrinter::getAnalysisUsage(AU);
  }

private:
  void generateNvInfoModule(MachineFunction &MF);

  /// @return constant0 size in byte
  unsigned generateNvInfo(MachineFunction &MF,
                          std::vector<std::unique_ptr<NvInfo>> &Info);

  /// Update instr/mbb offsets
  void scanFunction(MachineFunction *MF);

  /// Record MI offset in byte
  DenseMap<const MachineInstr*, uint64_t> MIOffsets;
  /// Record MBB offset in byte
  DenseMap<const MachineBasicBlock*, uint64_t> MBBOffsets;

  /// BasicBlockInfo - Information about the offset and size of a single
  /// basic block.
  struct BasicBlockInfo {
    /// Offset - Distance from the beginning of the function to the beginning
    /// of this basic block.
    ///
    /// The offset is always aligned as required by the basic block.
    unsigned Offset = 0;

    /// Size - Size of the basic block in bytes.  If the block contains
    /// inline assembly, this is a worst case estimate.
    ///
    /// The size does not include any alignment padding whether from the
    /// beginning of the block, or from an aligned jump table at the end.
    unsigned Size = 0;

    BasicBlockInfo() = default;

    /// Compute the offset immediately following this block.
    unsigned postOffset() const { return Offset + Size; }
  };

  SmallVector<BasicBlockInfo, 16> BlockInfo;

  const TargetInstrInfo *TII = nullptr;
};
} // anonymous namespace

// helper
// generate .nv.info (per module)
void GASSAsmPrinter::generateNvInfoModule(MachineFunction &MF) {
  //=--------------------Compute #Regs---------------------------------------=//
  const MachineRegisterInfo *MRI = &MF.getRegInfo();
  unsigned MaxReg = 0;
  // TODO: Do we need to consider v2/v4 registers here?
  for (unsigned Reg = GASS::VGPR0; Reg <= GASS::VGPR255; ++Reg) {
    if (MRI->isPhysRegUsed(Reg))
      MaxReg = Reg;
  }
  MaxReg -= GASS::VGPR0;
  while (MaxReg % 8 != 0) 
    ++MaxReg;
  //=------------------------------------------------------------------------=//
  // REGCOUNT
  ModuleInfo.emplace_back(createNvInfoRegCount(/*Count*/MaxReg, &MF));
  // MAX_STACK_SIZE
  // TODO: get this value from register allocator
  ModuleInfo.emplace_back(createNvInfoMaxStackSize(/*MaxSize*/0, &MF));
  // MIN_STACK_SIZE
  ModuleInfo.emplace_back(createNvInfoMinStackSize(0, &MF));
  // FRAME_SIZE
  ModuleInfo.emplace_back(createNvInfoFrameSize(0, &MF));
}

// compute offsets
void GASSAsmPrinter::scanFunction(MachineFunction *MF) {
  MF->RenumberBlocks();

  BlockInfo.clear();
  BlockInfo.resize(MF->getNumBlockIDs());

  // First, compute the size of all basic blocks.
  for (MachineBasicBlock &MBB : *MF) {
    uint64_t Size = 0;
    for (const MachineInstr &MI : MBB)
      Size += 16; // TII->getInstSizeInBytes(MI);
    BlockInfo[MBB.getNumber()].Size = Size;
  }

  // Second, update MBB offset
  unsigned PrevNum = MF->begin()->getNumber();
  assert(PrevNum == 0);
  for (auto &MBB : *MF) {
    unsigned Num = MBB.getNumber();
    if (Num == 0)
      continue;
    // Get the offset and known bits at the end of the layout predecessor.
    // Include the alignment of the current block.
    uint64_t Offset = BlockInfo[PrevNum].postOffset();
    BlockInfo[Num].Offset = Offset;

    MBBOffsets[&MBB] = BlockInfo[Num].Offset;

    PrevNum = Num;
  }

  // Update Instr offset
  for (MachineBasicBlock &MBB : *MF) {
    uint64_t Offset = MBBOffsets.lookup(&MBB);
    for (const MachineInstr &MI : MBB) {
      MIOffsets[&MI] = Offset;
      Offset += 16; // TII->getInstSizeInBytes(MI);
    }
  }

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
  for (int i = 0; i < Arguments.size(); ++i) {
    GASSArgument &Arg = Arguments[i];
    Info.emplace_back(createNvInfoKParamInfo(0, i, 
                                             Arg.Offset, Arg.Size, 
                                             calLog2(Arg.LogAlignment)));
  }
  Info.emplace_back(createNvInfoMaxRegCount(255));
  Info.emplace_back(createNvInfoExitInstrOffsets({EXITOffsets}));

  return ParamBaseOffset + CurrentParamOffset;
}

bool GASSAsmPrinter::runOnMachineFunction(MachineFunction &MF) {
  MF.setAlignment(Align(128));

  StallSetter = &getAnalysis<GASSStallSetting>();
  TII = MF.getSubtarget().getInstrInfo();

  // MF.dump();
  scanFunction(&MF);

  SetupMachineFunction(MF);
  emitFunctionBody();

  // Clear per-function data
  MIOffsets.clear();
  MBBOffsets.clear();
  return false;
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

  MCInstLowering.LowerToMCInst(MI, Inst, MBBOffsets,
                                         MIOffsets);
  // Update stall cycles
  {
    uint32_t Flags = Inst.getFlags();
    Flags &= ~(0b1111);
    unsigned Stalls = StallSetter->getStallCycles(MI);
    Flags |= (Stalls << 9); // so bad :(
    if (Stalls >= 10) {
      Flags &= ~(1<<13);  // Force yield // Even worse :)
    } else 
      Flags |= 1<<13;
    Inst.setFlags(Flags);
  }
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
  // tailing instructions (bra & nop)
  emitTailingInstructions();

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

void GASSAsmPrinter::emitTailingInstructions() {
  // BRA
  EmitToStreamer(*OutStreamer, MCInstBuilder(GASS::TailBRA).addReg(GASS::PT));
  CurrentOffset += 16;

  // Padding with NOPs
  while (CurrentOffset % 128 != 0) {
    EmitToStreamer(*OutStreamer, MCInstBuilder(GASS::NOP).addReg(GASS::PT));
    CurrentOffset += 16;
  }
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


//==------------------------------------------------------------------------==//
// Override void emitFunctionBody();
//   Just to emit TargetOpcode::IMPLICIT_DEF as NOP
//==------------------------------------------------------------------------==//