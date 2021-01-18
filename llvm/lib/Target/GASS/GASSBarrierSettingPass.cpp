// Setting wait barrier for instructions
//
// Highlevel design phylosophy:
//   Mimic register allocation
//   Allocate physical barrier to vritual barriers
// 
// So we need:
//   LiveBarMatrix (ref: LiveRegMatrix)
//
// Note:
// 1. Smem instr execution order is the same as the issuing order
// 2. Gmem instrs are executed out-of-order
// 3. If 
// 

#include "GASS.h"
#include "GASSSubtarget.h"
#include "GASSInstrInfo.h"
#include "LiveBarRange.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

#include <map>
#include <set>
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
class Barrier;

class GASSBarrierSetting : public MachineFunctionPass {
  const GASSInstrInfo *GII = nullptr;
  const GASSRegisterInfo *GRI = nullptr;
  const MachineRegisterInfo *MRI = nullptr;
  const LiveIntervals *LIS = nullptr;


  // Cache MBB
  MachineBasicBlock *CurMBB = nullptr;
public:
  static char ID;

  GASSBarrierSetting() : MachineFunctionPass(ID) {
    initializeGASSBarrierSettingPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  void runOnMachineBasicBlock(MachineBasicBlock &MBB);

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<LiveIntervals>();
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

  StringRef getPassName() const override {
    return GASS_BARRIERSETTING_NAME;
  }

private:
  // helper functions for merging and allocating barriers
  void coalesceLds(std::vector<Barrier> &Barriers);
  unsigned countBarriers(std::vector<Barrier> &Barriers);
  void mergeBarriers(std::vector<Barrier> &Barriers);
  void allocatePhysBarriers(std::vector<Barrier> &Barriers);
};

char GASSBarrierSetting::ID = 0;

class Barrier {
public:
  Barrier(BarrierType BT, MachineOperand *Src, MachineOperand *Dst, 
          SlotIndex SISrc, SlotIndex SIDst)
    : BT(BT), Src(Src), Dst(Dst), LBR(SISrc, SIDst) {
    if (BT == RAW_S || BT == RAW_G || BT == RAW_C)
      IsRead = true;
    else
      IsRead = false;

    // 
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

  const LiveBarRange& getLiveBarRange() const {
    return LBR;
  }

  void setPhysBarIdx(unsigned Idx) {
    PhysBarIdx = Idx;
    IsPhysical = true;
  }

  unsigned getPhysBarIdx() const {
    return PhysBarIdx;
  }

  void dump() const {
    dbgs() << *Src << " (line " << LBR.beginIndex() << ") -> "
            << *Dst << " (line " << LBR.endIndex() << ") "
            << " \t[PhysBarIdx: " << PhysBarIdx << ", " << getBTStr() << "]\n";
  }

private:
  BarrierType BT;
  // If this barrier is read barrier (for RAW)
  bool IsRead = false;
  bool IsPhysical = false;
  unsigned PhysBarIdx = 0;
  MachineOperand *Src = nullptr;
  MachineOperand *Dst = nullptr;
  LiveBarRange LBR;

  // helper function
  std::string const getBTStr() const {
  switch (BT) {
  case RAW_C : return "RAW_C";
  case RAW_S : return "RAW_S";
  case RAW_G : return "RAW_G";
  case WAR_G : return "WAR_G";
  case WAR_S : return "WAR_S";
  }
  }
};

// Simpler abstraction?
class LiveBarMatrix {
public:
  enum InterferenceKind {
    IK_Free = 0,
  };


};
} // anonymous 

namespace llvm {
  void initializeLiveIntervalsPass(PassRegistry&);
}

INITIALIZE_PASS_BEGIN(GASSBarrierSetting, DEBUG_TYPE,
                      GASS_BARRIERSETTING_NAME, false, false);
INITIALIZE_PASS_DEPENDENCY(LiveIntervals)
INITIALIZE_PASS_END(GASSBarrierSetting, DEBUG_TYPE,
                    GASS_BARRIERSETTING_NAME, false, false);

bool GASSBarrierSetting::runOnMachineFunction(MachineFunction &MF) {
  const GASSSubtarget &ST = MF.getSubtarget<GASSSubtarget>();
  GII = ST.getInstrInfo();
  GRI = ST.getRegisterInfo();
  MRI = &MF.getRegInfo();
  LIS = &getAnalysis<LiveIntervals>();

  // Use MachineRegisterInfo to get def-use chain
  // TODO: Get the LiveInterval
  for (MachineBasicBlock &MBB : MF) {
    runOnMachineBasicBlock(MBB);
  }

  return true;
}

void GASSBarrierSetting::runOnMachineBasicBlock(MachineBasicBlock &MBB) {
  CurMBB = &MBB;

  // Recored Barriers
  std::vector<Barrier> Barriers;

  // TODO: handle livein & liveout
  for (auto iter = MBB.begin(); iter != MBB.end(); ++iter) {
    MachineInstr &MI = *iter;

    // Provide default flag encoding
    GII->initializeFlagsEncoding(MI);
    // Check RAW dependency
    if (GII->isLoad(MI)) {
      MachineOperand *BSrc = MI.defs().begin();
      SlotIndex SISrc = LIS->getInstructionIndex(MI);
      for (MachineOperand &BDst : MRI->use_operands(BSrc->getReg())) {
        // Create Barrier
        SlotIndex SIDst = LIS->getInstructionIndex(*BDst.getParent());
        Barriers.emplace_back(/*TODO*/RAW_G, BSrc, &BDst, SISrc, SIDst);
      }
      // Create WAR for MemOperand
    } else if (GII->isStore(MI)) {
      // Create WAR for store instructions
    }

    // Check WAR dependency
    if (MachineOperand *BSrc = GII->getMemOperandReg(MI)) {
      SlotIndex SISrc = LIS->getInstructionIndex(MI);

      // if current instr writes to it, we don't need to care about that
      auto war_iter = iter;
      ++war_iter;
      for (; war_iter != MBB.end(); ++war_iter) {
        for (MachineOperand &Def : war_iter->defs()) {
          if (Def.isReg()) {
            if (GRI->regsOverlap(BSrc->getReg(), Def.getReg())) {
              SlotIndex SIDst = LIS->getInstructionIndex(*war_iter);
              Barriers.emplace_back(WAR_G, BSrc, &Def, SISrc, SIDst);
              goto TheEnd; // double break
            }
          }
        }
      }
      TheEnd: {}
    }
  }

  // 1. Coalesce smem 
  coalesceLds(Barriers);

  // 2. Merge if necessary
  while (countBarriers(Barriers) > kNumBarriers)
    mergeBarriers(Barriers);

  // 3. Allocate physical Barriers
  allocatePhysBarriers(Barriers);

  // 3.1 print allocated result (debug info)
  for (const Barrier &Bar : Barriers) {
    Bar.dump();
  }

  // 4. Encoding Barrier info
  for (const Barrier &Bar : Barriers)
    Bar.encodeBarrierInfo(GII);
}

//=-----------------------------------------------=//
// Private helper functions
//=-----------------------------------------------=//
void GASSBarrierSetting::coalesceLds(std::vector<Barrier> &Barriers) {
  // 
}

unsigned GASSBarrierSetting::countBarriers(std::vector<Barrier> &Barriers) {
  unsigned NumBars = 0;
  // Scan the MBB & count number of activate barriers at each point
  //   & record the max number
  for (const MachineInstr &MI : *CurMBB) {
    unsigned CurNumBars = 0; // Number of activate barriers at current point
    SlotIndex CurSI = LIS->getInstructionIndex(MI);
    for (const Barrier &Bar : Barriers) {
      if (Bar.getLiveBarRange().contains(CurSI)) 
        CurNumBars++;
    }
    NumBars = std::max(NumBars, CurNumBars);
  }
  return NumBars;
}

void GASSBarrierSetting::mergeBarriers(std::vector<Barrier> &Barriers){
  // TODO: fill this.
  llvm_unreachable("mergeBarriers() has not been implemented");
}

void GASSBarrierSetting::allocatePhysBarriers(std::vector<Barrier> &Barriers) {
  std::set<unsigned> FreePhysBar;
  for (unsigned i=0; i<kNumBarriers; ++i) FreePhysBar.insert(i);

  using BarIter = std::vector<Barrier>::iterator;

  std::map<BarIter, unsigned> ActiveBarriers; // Barrier->PhysBar
  std::set<BarIter> RemainingBarriers;

  for (BarIter it = Barriers.begin(); it != Barriers.end(); ++it) {
    RemainingBarriers.insert(it);
  }

  // Scan the basic block. Allocate phys barriers.
  for (const MachineInstr &MI : *CurMBB) {
    // TODO: can't we use loop idx? Maybe we should sort Barriers?
    SlotIndex CurSI = LIS->getInstructionIndex(MI); 
    // 1. Expire old
    // Note: the first instruction shouldn't wait on barriers
    for (auto it = ActiveBarriers.begin(); it != ActiveBarriers.end(); ) {
      if (it->first->getLiveBarRange().expireAt(CurSI)) {
        unsigned PhysBarIdx = it->second;
        FreePhysBar.insert(PhysBarIdx);
        it = ActiveBarriers.erase(it);
      } else 
        ++it;
    }

    // 2. Allocate new
    for (auto it = RemainingBarriers.begin(); it != RemainingBarriers.end(); ) {
      if ((*it)->getLiveBarRange().liveAt(CurSI)) {
        auto free_phys_bar = FreePhysBar.begin();
        ActiveBarriers.emplace(*it, *free_phys_bar);
        // Set PhysBarIdx
        FreePhysBar.erase(free_phys_bar);
        (*it)->setPhysBarIdx(*free_phys_bar);
        it = RemainingBarriers.erase(it);
      } else 
        ++it;
    }
  }
}

//=---------End of private helper functions-------=//

FunctionPass *llvm::createGASSBarrierSettingPass() {
  return new GASSBarrierSetting();
}