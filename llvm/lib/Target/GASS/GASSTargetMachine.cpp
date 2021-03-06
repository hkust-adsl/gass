#include "GASS.h"
#include "GASSTargetMachine.h"
#include "GASSTargetTransformInfo.h"
#include "GASSISelDAGToDAG.h"
#include "GASSSchedStrategy.h"
#include "GASSScheduleDAGMutations.h"
#include "TargetInfo/GASSTargetInfo.h"
#include "llvm/Analysis/CFGPrinter.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/Verifier.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/ConstantHoisting.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Vectorize.h"
#include <memory>

using namespace llvm;

static cl::opt<bool> BenchmarkMode("gass-benchmark-mode",
                                   cl::desc("Enable Benchmark Mode"),
                                   cl::init(false), cl::Hidden);

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeGASSTarget() {
  RegisterTargetMachine<GASSTargetMachine> X(getTheGASSTarget());
  auto *PR = PassRegistry::getPassRegistry();
  initializeGASSStallSettingPass(*PR);
  // initializeGASSBranchOffsetPass(*PR);
  initializeGASSExpandPreRAPseudoPass(*PR);
  initializeGASSAnnotateUniformValuesPass(*PR);
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
      Subtarget(std::make_unique<GASSSubtarget>(TT, CPU, FS, *this)),
      TLOF(std::make_unique<GASSTargetObjectFile>()) {
  // TODO: check this.
  setRequiresStructuredCFG(true);
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

  // Major APIs
  // void addISelPasses(); // Do NOT override this
  void addMachinePasses() override;

  void addIRPasses() override;

  // Any "last minute" IR passes 
  // (which are run just before instruction selector).
  bool addPreISel() override;

  bool addInstSelector() override;

  // Machine Passes
  void addMachineSSAOptimization() override;

  // GASS needs custom regalloc pipeline. (GASSIfConvert after RegisterCoalesce)
  void addOptimizedRegAlloc() override;

  // Set instruction control info
  void addPreEmitPass() override;

  // // override MachineScheduleStrategy
  // ScheduleDAGInstrs *
  // createMachineScheduler(MachineSchedContext *C) const override {
  //   ScheduleDAGMILive *DAG =
  //       new ScheduleDAGMILive(C, std::make_unique<GASSSchedStrategy>(C));
  //   DAG->addMutation(createGASSSM80DepRemoveDAGMutation());
  //   // DAG->addMutation(createGASSTensorCoreChainDAGMutation());
  //   // DAG->addMutation(createGASSCarryInClusterDAGMutation());
  //   return DAG;
  // }
};
} // anonymous namespace

TargetPassConfig *GASSTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new GASSPassConfig(*this, PM);
}

// Major API
void GASSPassConfig::addMachinePasses() {
  AddingMachinePasses = true;

  // Machine - CSE/DCE/LICM/Peephole
  addMachineSSAOptimization();

  // Debugifying the register allocator passes seems to provoke some
  // non-determinism that affects CodeGen and there doesn't seem to be a point
  // where it becomes safe again so stop debugifying here.
  DebugifyIsSafe = false;

  // Run register allocation and passes that are tightly coupled with it,
  // including phi elimination and scheduling.
  addOptimizedRegAlloc();

  // Prolog/Epilog inserter needs a TargetMachine to instantiate. But only
  // do so if it hasn't been disabled, substituted, or overridden.
  if (!isPassSubstitutedOrOverridden(&PrologEpilogCodeInserterID))
      addPass(createPrologEpilogInserterPass());

  // Expand pseudo instructions before second scheduling pass.
  addPass(&ExpandPostRAPseudosID);

  addBlockPlacement();

  // Delete IMPLICIT_DEF & Set barriers
  addPreEmitPass();

  AddingMachinePasses = false;
}

void GASSPassConfig::addIRPasses() {
  addPass(createDeadCodeEliminationPass());
  addPass(createGASSDeleteDeadPHIsPass());

  // Can be useful (following NVPTX)
  // addPass(createGVNPass());
  // or
  addPass(createEarlyCSEPass());
  
  addPass(createGASSAddrSpacePass()); // required by infer address space
  addPass(createSROAPass());
  addPass(createInferAddressSpacesPass());

  // LSR and other generic IR passes
  // TargetPassConfig::addIRPasses();
  // LSR leads to incorrect result in some cases (?)
  {
    addPass(createVerifierPass());

    // LSR here. We don't need them (?)
    addPass(createCanonicalizeFreezeInLoopsPass());
    // debug
    // addPass(createGASSIVDebugPass());
    // addPass(createLoopStrengthReducePass());

    // GC lowering?
    addPass(createLowerConstantIntrinsicsPass());
    addPass(createConstantHoistingPass());

    addPass(createUnreachableBlockEliminationPass());

    // addPass(createConstantHoistingPass());
    // Some other passes...
  }

  addPass(createGASSCodeGenPreparePass());
  addPass(createLoadStoreVectorizerPass());
  // Following the practice in AArch64
  // TODO: disable it for now.
  // addPass(createLICMPass());

  // addPass(createSinkingPass());
}

bool GASSPassConfig::addPreISel() {
  // TODO: can we eliminate this?
  // addPass(createGASSSinkingPass());
  // following the practice in AMDGPU
  // addPass(createGASSAnnotateUniformValues());
  return false;
}

bool GASSPassConfig::addInstSelector() {
  addPass(new GASSDAGToDAGISel(&getGASSTargetMachine()));

  addPass(createGASSConstantMemPropagatePass()); 
  addPass(createGASSMachineDCEPass());
  
  addPass(createGASSExpandPreRAPseudoPass());
  return false;
}

// Machine passes
void GASSPassConfig::addMachineSSAOptimization() {
  // Optimize PHIs before DCE: removing dead PHI cycles may make more
  // instructions dead.
  addPass(&OptimizePHIsID);

  addPass(&EarlyMachineLICMID);
  addPass(&MachineCSEID);
  addPass(&MachineSinkingID);

  // I don't understand how LLVM's peephole pass works
}

void GASSPassConfig::addOptimizedRegAlloc() {
  addPass(&DetectDeadLanesID);

  // TODO: what's this?
  addPass(&ProcessImplicitDefsID);

  // LiveVariables currently requires pure SSA form.
  //
  // FIXME: Once TwoAddressInstruction pass no longer uses kill flags,
  // LiveVariables can be removed completely, and LiveIntervals can be directly
  // computed. (We still either need to regenerate kill flags after regalloc, or
  // preferably fix the scavenger to not depend on them).
  // FIXME: UnreachableMachineBlockElim is a dependant pass of LiveVariables.
  // When LiveVariables is removed this has to be removed/moved either.
  // Explicit addition of UnreachableMachineBlockElim allows stopping before or
  // after it with -stop-before/-stop-after.
  addPass(&UnreachableMachineBlockElimID);
  addPass(&LiveVariablesID);

  // Edge splitting is smarter with machine loop info.
  addPass(&MachineLoopInfoID);
  addPass(&PHIEliminationID);

  // Eventually, we want to run LiveIntervals before PHI elimination.
  // if (EarlyLiveIntervals)
  //   addPass(&LiveIntervalsID, false);

  addPass(&TwoAddressInstructionPassID);
  addPass(&RegisterCoalescerID); // Standard

  // The machine scheduler may accidentally create disconnected components
  // when moving subregister definitions around, avoid this by splitting them to
  // separate vregs before. Splitting can also improve reg. allocation quality.
  addPass(&RenameIndependentSubregsID);

  // addPass(&MachineFunctionPrinterPassID);

  //==***************** GASS specific *************************==//
  // PreRA IfConvert
  addPass(createGASSIfConversionPass());
  //==*********************************************************==//
  addPass(&DeadMachineInstructionElimID);

  // PreRA instruction scheduling.
  addPass(&MachineSchedulerID);

  addPass(createGASSMarkUndeadPass());
  // addPass(createGASSIVDebugPass());

  // // Compute Register Pressure at each line
  // addPass(createRegPressureComputePass());
  if (addRegAssignmentOptimized()) {
    // Allow targets to expand pseudo instructions depending on the choice of
    // registers before MachineCopyPropagation.
    addPostRewrite();

    // Copy propagate to forward register uses and try to eliminate COPYs that
    // were not coalesced.
    addPass(&MachineCopyPropagationID);

    // Run post-ra machine LICM to hoist reloads / remats.
    //
    // FIXME: can this move into MachineLateOptimization?
    addPass(&MachineLICMID);
  }
}


// NVGPU specific passes
void GASSPassConfig::addPreEmitPass() {
  // This pass changes IMPLICT_DEF to NOP
  addPass(createGASSPreEmitPreparePass());
  // A schedule pass to sink LDGs
  // addPass(createGASSLDGSinkPass());
  // Debug pass
  // addPass(createGASSMachineFunctionCFGPrinterPass());
  // addPass(createGASSPhysRegLivenessPass());
  addPass(createGASSBarrierSettingPass());
  // StallSetting here (called by GASSAsmPrinter)
}
