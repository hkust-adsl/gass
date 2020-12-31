#include "GASS.h"
#include "GASSTargetMachine.h"
#include "GASSInstructionSelect.h"
#include "llvm/CodeGen/GlobalISel/IRTranslator.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelect.h"
#include "llvm/CodeGen/GlobalISel/Legalizer.h"
#include "llvm/CodeGen/GlobalISel/RegBankSelect.h"

using namespace llvm;

GASSTargetMachine::GASSTargetMachine(const Target &T, const Triple &TT,
                                     StringRef CPU, StringRef FS, 
                                     const TargetOptions &Options,
                                     Optional<Reloc::Model> RM,
                                     Optional<CodeModel::Model> CM,
                                     CodeGenOpt::Level OL, bool JIT)
    : LLVMTargetMachine(T, std::string("e-i64:64-i128:128-v16:16-v32:32-n16:32:64"), 
                        TT, CPU, FS, Options, Reloc::PIC_,
                        getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(std::make_unique(GASSTargetObjectFile())),
      Subtarget(TT, CPU, FS, *this) {
  initAsmInfo(); // What does this do?
}

GASSTargetMachine::~GASSTargetMachine() = default;

TargetTransformInfo
GASSTargetMachine::getTargetTransformInfo(const Function &F) {
  return TargetTransformInfo(GASSTTIImpl(this, F));
}

//=---------------------------------------------=//
// PassConfig
//=---------------------------------------------=//
namespace {
class GASSPassConfig : public TargetPassConfig {
public:
  GASSPassConfig(GASSTargetMachine &TM, PassManagerBase &PM)
    : TargetPassConfig(TM, PM);

  // Required by ISel
  GASSTargetMachine &getGASSTargetMachine() const {
    return getTM<GASSTargetMachine>();
  }

  void addIRPasses() override;
  // GISel
  bool addIRTranslator() override;
  bool addLegalizeMachineIR() override;
  bool addRegBankSelect() override;
  bool addGlobalInstructionSelect() override;
  // End of GISel
  void addPreRegAlloc() override;
  void addPostRegAlloc() override;



  // FunctionPass *createTargetRegisterAllocator(bool) override;
  // void addFastRegAlloc() override;
  // void addOptimizedRegAlloc() override;
};
} // anonymous namespace

TargetPassConfig *GASSTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new GASSPassConfig(*this, PM);
}

void GASSPassConfig::addIRPasses() {
  addPass(createGASSAddrSpacePass()); // required by infer address space
  // SROA
  addPass(createSROAPass());
  addPass(createInferAddressSpacePass());

  // LSR and other generic IR passes
  TargetPassConfig::addIRPasses();

  addPass(createLoadStoreVectorizerPass());
}

//=-------------------------------------------=//
// GISel
//=-------------------------------------------=//
bool GASSPassConfig::addIRTranslator() {
  addPas(new IRTranslator(getOptLevel()));
  return false;
}

bool GASSPassConfig::addLegalizeMachineIR() {
  addPass(new Legalizer());
  return false;
}

bool GASSPassConfig::addRegBankSelect() {
  addPass(new RegBankSelect());
  return false;
}

bool GASSPassConfig::addGlobalInstructionSelect() {
  addPass(GASSInstructionSelector());
  return false;
}

