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
#include "llvm/InitializePasses.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/ConstantHoisting.h"
#include "llvm/Transforms/Scalar/GVN.h"
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

  void addIRPasses() override;

  // Any "last minute" IR passes 
  // (which are run just before instruction selector).
  bool addPreISel() override;

  bool addInstSelector() override;
  bool addILPOpts() override;

  //=---------------------------------------=//
  // Debug, to delete
  // Print cfg
  void addMachineLateOptimization() override;
  void addPreRegAlloc() override;
  //=--------------------------------------=//

  // GASS needs custom regalloc pipeline. (GASSIfConvert after RegisterCoalesce)
  void addOptimizedRegAlloc() override;

  void addPreSched2() override;

  // Set instruction control info
  void addPreEmitPass() override;

  // // override MachineScheduleStrategy
  ScheduleDAGInstrs *
  createMachineScheduler(MachineSchedContext *C) const override {
    ScheduleDAGMILive *DAG =
        new ScheduleDAGMILive(C, std::make_unique<GASSSchedStrategy>(C));
    // DAG->addMutation(createGASSTensorCoreChainDAGMutation());
    // DAG->addMutation(createGASSCarryInClusterDAGMutation());
    return DAG;
  }
};
} // anonymous namespace

TargetPassConfig *GASSTargetMachine::createPassConfig(PassManagerBase &PM) {
  return new GASSPassConfig(*this, PM);
}

void GASSPassConfig::addIRPasses() {
  addPass(createDeadCodeEliminationPass());

  // Can be useful (following NVPTX)
  // addPass(createGVNPass());
  // or
  addPass(createEarlyCSEPass());
  
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

  addPass(createSinkingPass());
}

bool GASSPassConfig::addPreISel() {
  // TODO: can we eliminate this?
  addPass(createGASSSinkingPass());
  return false;
}

bool GASSPassConfig::addInstSelector() {
  addPass(new GASSDAGToDAGISel(getGASSTargetMachine()));
  addPass(createGASSExpandPreRAPseudoPass());
  return false;
}

void GASSPassConfig::addOptimizedRegAlloc() {
  addPass(&DetectDeadLanesID, false);

  addPass(&ProcessImplicitDefsID, false);

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
  addPass(&UnreachableMachineBlockElimID, false);
  addPass(&LiveVariablesID, false);

  // Edge splitting is smarter with machine loop info.
  addPass(&MachineLoopInfoID, false);
  addPass(&PHIEliminationID, false);

  // Eventually, we want to run LiveIntervals before PHI elimination.
  // if (EarlyLiveIntervals)
  //   addPass(&LiveIntervalsID, false);

  addPass(&TwoAddressInstructionPassID, false);
  addPass(&RegisterCoalescerID);

  // The machine scheduler may accidentally create disconnected components
  // when moving subregister definitions around, avoid this by splitting them to
  // separate vregs before. Splitting can also improve reg. allocation quality.
  addPass(&RenameIndependentSubregsID);

  //==***************** GASS specific *************************==//
  // PreRA IfConvert
  // addPass(createMachineVerifierPass("** Verify Before Early If Conversion **"));
  addPass(createGASSIfConversionPass());
  // FIXME: Do we need to update LiveIntervals?
  // Now we have more chances to do CSE   
  // Not in SSA form
  // addPass(&MachineCSEID);
  //==*********************************************************==//

  // PreRA instruction scheduling.
  addPass(&MachineSchedulerID);

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

bool GASSPassConfig::addILPOpts() {
  // addPass(&EarlyIfPredicatorID);
  return false;
}

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
  // A schedule pass to sink LDGs
  // addPass(createGASSLDGSinkPass());
  // Debug pass
  // addPass(createGASSMachineFunctionCFGPrinterPass());
  addPass(createGASSBarrierSettingPass());
  // StallSetting here (called by GASSAsmPrinter)
}