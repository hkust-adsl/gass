#include "GASS.h"
#include "GASSTargetMachine.h"
#include "GASSTargetTransformInfo.h"
#include "GASSISelDAGToDAG.h"
#include "TargetInfo/GASSTargetInfo.h"
#include "llvm/Analysis/CFGPrinter.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/ConstantHoisting.h"
#include "llvm/Transforms/Vectorize.h"
#include <memory>

using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeGASSTarget() {
  RegisterTargetMachine<GASSTargetMachine> X(getTheGASSTarget());
  auto PR = PassRegistry::getPassRegistry();
  initializeGASSStallSettingPass(*PR);
  // initializeGASSBranchOffsetPass(*PR);
  initializeGASSExpandPreRAPseudoPass(*PR);
}

GASSTargetMachine::GASSTargetMachine(const Target &T, const Triple &TT,
                                     StringRef CPU, StringRef FS, 
                                     const TargetOptions &Options,
                                     Optional<Reloc::Model> RM,
                                     Optional<CodeModel::Model> CM,
                                     CodeGenOpt::Level OL, bool JIT)
    : LLVMTargetMachine(T, 
                        std::string("e-p3:32:32-i64:64-i128:128-v16:16-v32:32-n16:32:64"), 
                        TT, CPU, FS, Options, Reloc::PIC_,
                        getEffectiveCodeModel(CM, CodeModel::Small), OL),
      TLOF(std::make_unique<GASSTargetObjectFile>()),
      Subtarget(std::make_unique<GASSSubtarget>(TT, CPU, FS, *this)) {
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
    : TargetPassConfig(TM, PM) {}

  // Required by ISel
  GASSTargetMachine &getGASSTargetMachine() const {
    return getTM<GASSTargetMachine>();
  }

  void addIRPasses() override;
  bool addInstSelector() override;
  // bool addILPOpts() override;

  //=---------------------------------------=//
  // Debug, to delete
  // Print cfg
  void addMachineLateOptimization() override;
  void addPreRegAlloc() override;
  //=--------------------------------------=//

  void addPreSched2() override;

  // Set instruction control info
  void addPreEmitPass() override;
};
} // anonymous namespace

TargetPassConfig *GASSTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new GASSPassConfig(*this, PM);
}

void GASSPassConfig::addIRPasses() {
  addPass(createDeadCodeEliminationPass());
  addPass(createGASSAddrSpacePass()); // required by infer address space
  addPass(createSROAPass());
  addPass(createInferAddressSpacesPass());

  // LSR and other generic IR passes
  TargetPassConfig::addIRPasses();

  addPass(createGASSCodeGenPreparePass());
  addPass(createLoadStoreVectorizerPass());
  // Following the practice in AArch64
  // TODO: disable it for now.
  // addPass(createLICMPass());
}

bool GASSPassConfig::addInstSelector() {
  addPass(new GASSDAGToDAGISel(getGASSTargetMachine()));
  addPass(createGASSExpandPreRAPseudoPass());
  return false;
}

// bool GASSPassConfig::addILPOpts() {
//   // TODO: doesn't seem to work. should delete this.
//   // addPass(&EarlyIfConverterID);

//   return false;
// }

void GASSPassConfig::addPreRegAlloc() {
  // addPass(createGASSMachineFunctionCFGPrinterPass());
}

void GASSPassConfig::addMachineLateOptimization() {
  // Branch folding must be run after regalloc and prolog/epilog insertion.
  addPass(&BranchFolderPassID);

  // Tail duplication.
  // Note that duplicating tail just increases code size and degrades
  // performance for targets that require Structured Control Flow.
  // In addition it can also make CFG irreducible. Thus we disable it.
  if (!TM->requiresStructuredCFG())
    addPass(&TailDuplicateID);

  // Copy propagation.
  addPass(&MachineCopyPropagationID);
}

void GASSPassConfig::addPreSched2() {
  // addPass(createIfConverter([](const MachineFunction &MF) {
  //   return true;
  // }));
  // addPass(createGASSIfConversionPass());
  // addPass(createGASSMachineFunctionCFGPrinterPass());
}

// NVGPU specific passes
void GASSPassConfig::addPreEmitPass() {
  // This pass changes IMPLICT_DEF to NOP
  addPass(createGASSPreEmitPreparePass());
  // Debug pass
  // addPass(createGASSMachineFunctionCFGPrinterPass());
  addPass(createGASSBarrierSettingPass());
  // StallSetting here (called by GASSAsmPrinter)
}