// Setting wait barrier for instructions
// Note:
// 1. Smem instr execution order is the same as the issuing order
// 2. Gmem instrs are executed out-of-order
// 3. If 

#include "GASS.h"
#include "GASSSubtarget.h"
#include "GASSInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

#include <unordered_set>
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "gass-barrier-setting"

#define GASS_BARRIERSETTING_NAME "Setting Instruction Wait Barriers"

enum BarrierType {
  RAW_S,
  RAW_G,
  RAW_C,
  WAR_S,
  WAR_G
};

constexpr unsigned kNumBarriers = 6;

namespace {
class GASSBarrierSetting : public MachineFunctionPass {
  const GASSInstrInfo *GII = nullptr;
  const GASSRegisterInfo *GRI = nullptr;
  const MachineRegisterInfo *MRI = nullptr;
public:
  static char ID;

  GASSBarrierSetting() : MachineFunctionPass(ID) {
    initializeGASSBarrierSettingPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void runOnMachineBasicBlock(MachineBasicBlock &MBB);

  // void getAnalysisUsage(AnalysisUsage &AU) const override {
  //   AU.addRequired<LiveIntervals>();
  //   AU.setPreservesAll();
  //   MachineFunctionPass::getAnalysisUsage(AU);
  // }

  StringRef getPassName() const override {
    return GASS_BARRIERSETTING_NAME;
  }
};

char GASSBarrierSetting::ID = 0;

class Barrier {
public:
  Barrier(BarrierType BT, MachineOperand *Src, MachineOperand *Dst)
    : BT(BT), Src(Src), Dst(Dst) {
    if (BT == RAW_S || BT == RAW_G || BT == RAW_C)
      IsRead = true;
    else
      IsRead = false;
  }

  void encodeBarrierInfo(const GASSInstrInfo *GII) const {
    MachineInstr *SrcInstr = Src->getParent();
    MachineInstr *DstInstr = Dst->getParent();

    // MachineInstr has a field named flags (uint16_t)
    if (IsRead)
      GII->encodeReadBarrier(*SrcInstr, PhysBarIdx);
    else 
      GII->encodeWriteBarrier(*SrcInstr, PhysBarIdx);

    GII->encodeBarrierMask(*DstInstr, PhysBarIdx);
  }

private:
  BarrierType BT;
  // If this barrier is read barrier (for RAW)
  bool IsRead = false;
  unsigned PhysBarIdx = 0;
  MachineOperand *Src = nullptr;
  MachineOperand *Dst = nullptr;
};
} // anonymous 

namespace llvm {
  // void initializeLiveIntervalsPass(PassRegistry&);
}

INITIALIZE_PASS_BEGIN(GASSBarrierSetting, DEBUG_TYPE,
                      GASS_BARRIERSETTING_NAME, false, false);
// INITIALIZE_PASS_DEPENDENCY(LiveIntervals)
INITIALIZE_PASS_END(GASSBarrierSetting, DEBUG_TYPE,
                    GASS_BARRIERSETTING_NAME, false, false);

bool GASSBarrierSetting::runOnMachineFunction(MachineFunction &MF) {
  const GASSSubtarget &ST = MF.getSubtarget<GASSSubtarget>();
  GII = ST.getInstrInfo();
  GRI = ST.getRegisterInfo();
  MRI = &MF.getRegInfo();

  // Use MachineRegisterInfo to get def-use chain
  // TODO: Get the LiveInterval
  for (MachineBasicBlock &MBB : MF) {
    runOnMachineBasicBlock(MBB);
  }

  return true;
}

void GASSBarrierSetting::runOnMachineBasicBlock(MachineBasicBlock &MBB) {
  // Track a set of empty barriers
  std::unordered_set<unsigned> EmptyBarriers;
  for (unsigned i=0; i < kNumBarriers; ++i) EmptyBarriers.insert(i);

  // Recored Barriers
  std::vector<Barrier> Barriers;

  // TODO: handle livein & liveout
  for (MachineInstr &MI : MBB) {
    // Check RAW dependency
    if (GII->isLoad(MI)) {
      MachineOperand *BSrc = MI.defs().begin();
      for (MachineOperand &BDst : MRI->def_operands(BSrc->getReg())) {
        // Create Barrier
        Barriers.emplace_back(RAW_G, BSrc, &BDst);
      }
      // Create WAR for MemOperand
    } else if (GII->isStore(MI)) {
      // Create WAR for store instructions
      MI.dump();
      outs() << " is a store instr\n\n";
    }

    // Check WAR dependency
    for (const MachineOperand &MOP : MI.operands()) {
    }
  }

  // Allocate physical Barriers

  // Encoding Barrier info
  for (const Barrier &Bar : Barriers)
    Bar.encodeBarrierInfo(GII);
}

FunctionPass *llvm::createGASSBarrierSettingPass() {
  return new GASSBarrierSetting();
}