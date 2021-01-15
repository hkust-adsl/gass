#include "GASS.h"
#include "GASSInstrInfo.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineDominanceFrontier.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/RDFGraph.h"
#include "llvm/CodeGen/RDFLiveness.h"

using namespace llvm;

#define DEBUG_TYPE "gass-barrier-setting"

#define GASS_BARRIERSETTING_NAME "Setting Instruction Wait Barriers"

namespace {
class GASSBarrierSetting : public MachineFunctionPass {
public:
  static char ID;

  GASSBarrierSetting() : MachineFunctionPass(ID) {
    initializeGASSBarrierSettingPass(*PassRegistry::getPassRegistry());
  }

  // Required
  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachineDominatorTree>();
    AU.addRequired<MachineDominanceFrontier>();
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return GASS_BARRIERSETTING_NAME;
  }
};

char GASSBarrierSetting::ID = 0;
} // anonymous 

namespace llvm {
  void initializeMachineDominatorTreePass(PassRegistry&);
  void initializeMachineDominanceFrontierPass(PassRegistry&);
}

INITIALIZE_PASS_BEGIN(GASSBarrierSetting, DEBUG_TYPE,
                      GASS_BARRIERSETTING_NAME, false, false);
INITIALIZE_PASS_DEPENDENCY(MachineDominatorTree)
INITIALIZE_PASS_DEPENDENCY(MachineDominanceFrontier)
INITIALIZE_PASS_END(GASSBarrierSetting, DEBUG_TYPE,
                    GASS_BARRIERSETTING_NAME, false, false);

bool GASSBarrierSetting::runOnMachineFunction(MachineFunction &MF) {
  const TargetSubtargetInfo &ST = MF.getSubtarget();
  const TargetInstrInfo *TII = ST.getInstrInfo();
  const TargetRegisterInfo *TRI = ST.getRegisterInfo();
  
  MachineDominatorTree *MDT = &getAnalysis<MachineDominatorTree>();
  const auto &MDF = getAnalysis<MachineDominanceFrontier>();

  rdf::TargetOperandInfo TOI(*TII);
  rdf::DataFlowGraph G(MF, *TII, *TRI, *MDT, MDF, TOI);
  G.build();

  // Debug
  // errs() << rdf::PrintNode<rdf::FuncNode*>(G.getFunc(), G) << "\n";

  MachineRegisterInfo *MRI = &MF.getRegInfo();

  rdf::Liveness LV(*MRI, G);
  LV.trace(true); // dump result?
  LV.computeLiveIns();
  LV.resetLiveIns();
  LV.resetKills();

  return true;
}

FunctionPass *llvm::createGASSBarrierSettingPass() {
  return new GASSBarrierSetting();
}