#include "GASS.h"
#include "GASSTargetMachine.h"
#include "GASSTargetTransformInfo.h"
#include "GASSISelDAGToDAG.h"
#include "TargetInfo/GASSTargetInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Vectorize.h"
#include <memory>

using namespace llvm;

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeGASSTarget() {
  RegisterTargetMachine<GASSTargetMachine> X(getTheGASSTarget());
}

GASSTargetMachine::GASSTargetMachine(const Target &T, const Triple &TT,
                                     StringRef CPU, StringRef FS, 
                                     const TargetOptions &Options,
                                     Optional<Reloc::Model> RM,
                                     Optional<CodeModel::Model> CM,
                                     CodeGenOpt::Level OL, bool JIT)
    : LLVMTargetMachine(T, std::string("e-i64:64-i128:128-v16:16-v32:32-n16:32:64"), 
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
};
} // anonymous namespace

TargetPassConfig *GASSTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new GASSPassConfig(*this, PM);
}

void GASSPassConfig::addIRPasses() {
  // addPass(createGASSAddrSpacePass()); // required by infer address space
  // SROA
  addPass(createSROAPass());
  addPass(createInferAddressSpacesPass());

  // LSR and other generic IR passes
  TargetPassConfig::addIRPasses();

  addPass(createLoadStoreVectorizerPass());
}

bool GASSPassConfig::addInstSelector() {
  addPass(new GASSDAGToDAGISel(getGASSTargetMachine()));
  return false;
}