//==-------------------------------------------------------------------==//
//
// Setting wait barrier for instructions
//
// Highlevel design phylosophy:
//   Mimic register allocation
//   Allocate physical barrier to vritual barriers
//
// Note:
// 1. Smem instr execution order is the same as the issuing order
// 2. Gmem instrs are executed out-of-order
// 3. If 
// 
//==--------------------------------------------------------------------==//

#include "GASS.h"
#include "GASSSubtarget.h"
#include "GASSInstrInfo.h"
#include "LiveBarRange.h"
#include "MCTargetDesc/GASSMCTargetDesc.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineFunctionPass.h"

#include <map>
#include <set>
#include <vector>
#include <algorithm>

using namespace llvm;

#define DEBUG_TYPE "gass-barrier-setting"
#define GASS_BARRIERSETTING_NAME "Setting Instruction Wait Barriers"

enum PickBarAlg {
  PB_ALG_ORDERED,
  PB_ALG_ESTCOST,
  PB_ALG_MAXDEGREE,
};

static cl::opt<PickBarAlg> PickBarPairAlg(
  "gass-pick-bar-pair-alg",
  cl::Hidden,
  cl::desc("Algorithm to pick wait barrier pair to merge"),
  cl::values(
    clEnumValN(PB_ALG_ORDERED, "ordered", "first merge ldc then lds then ldg"),
    clEnumValN(PB_ALG_ORDERED, "est-cost", 
               "pick barriers pair with min merge cost"),
    clEnumValN(PB_ALG_MAXDEGREE, "max-degree", 
               "pick barrier pair with max degree in the interference graph")),
  cl::init(PB_ALG_ORDERED));

// TODO: should query SchedulerModel
enum BarrierType {
  RAW_S,
  RAW_G,
  RAW_C,
  RAW_TC,
  RAW_SFU,
  WAR_S,
  WAR_G,
  WAR_MEM,
};


// TODO: better to query subtarget
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

  // Reserved physical barriers, e.g., 0 for LDGDEPBAR & DEPBAR
  std::set<unsigned> Reserved;
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
  unsigned countBarriers(std::vector<Barrier> &Barriers);
  void allocatePhysBarriers(std::vector<Barrier> &Barriers);
};

char GASSBarrierSetting::ID = 0;

class Barrier {
  // Cache LIS, GII
  const LiveIntervals *LIS = nullptr;
  const GASSInstrInfo *GII = nullptr;
public:
  Barrier(MachineInstr &MI, SlotIndex SIStart, SlotIndex SIEnd, bool IsMemOp,
          const LiveIntervals *LIS, const GASSInstrInfo *GII)
    : LIS(LIS), GII(GII), End(SIEnd), LBR(SIStart, SIEnd) {
    Starts.push_back(SIStart);

    if (IsMemOp) BT = WAR_MEM;
    else {
      if (GII->isLDG(MI)) { BT = RAW_G; }
       else if (GII->isLDS(MI) || GII->isSHFL(MI)) { BT = RAW_S; }
       else if (GII->isLDC(MI)) { BT = RAW_C; }
       else if (GII->isTC(MI)) { BT = RAW_TC; }
       else if (GII->isSFU(MI)) { BT = RAW_SFU; }
       else if (GII->isSTS(MI)) { BT = WAR_S; }
       else if (GII->isSTG(MI)) { BT = WAR_G; }
       else { llvm_unreachable("Invalid MI for barrier"); }
    }

    if (BT == RAW_S || BT == RAW_G || BT == RAW_C || BT == RAW_TC || 
        BT == RAW_SFU)
      IsRAW = true;
    else
      IsRAW = false;
  }

  // Merge two barriers
  void merge(const Barrier &Other) {
    assert(IsRAW == Other.isRAW() && "Cannot merge RAW & WAR barriers");
    if ((BT == RAW_S && Other.getBarrierType() == RAW_S) ||
        (BT == RAW_TC && Other.getBarrierType() == RAW_TC) ||
        (BT == RAW_SFU && Other.getBarrierType() == RAW_SFU)) {
      // Since LDSs are executed in order, we only need one start point
      assert(Starts.size() == 1 && Other.getStarts().size() == 1);
      Starts[0] = std::max(Starts[0], Other.getStarts()[0]);
    } // TODO: merge can be different depending on Barrier Type
    else {
      Starts.insert(Starts.end(), 
                    Other.getStarts().begin(), Other.getStarts().end());
      std::sort(Starts.begin(), Starts.end());
    }

    End = std::min(End, Other.getEnd());

    // Update LiveBarRange
    LBR = LiveBarRange(Starts[0], End);
  }

  bool overlaps(const Barrier &Other) {
    return LBR.overlaps(Other.getLiveBarRange());
  }

  // accessors
  bool isRAW() const { return IsRAW; }

  BarrierType getBarrierType() const { return BT; }

  bool isInOrderRAW() const {
    if (BT == RAW_C || BT == RAW_S || BT == RAW_TC || BT == RAW_SFU)
      return true;
  }

  void encodeBarrierInfo(const GASSInstrInfo *GII) const {
    // MachineInstr has a field named flags (uint16_t)
    for (SlotIndex SIStart : Starts) {
      MachineInstr *StartInstr = LIS->getInstructionFromIndex(SIStart);
      if (IsRAW)
        GII->encodeWriteBarrier(*StartInstr, PhysBarIdx);
      else 
        GII->encodeReadBarrier(*StartInstr, PhysBarIdx);
    }

    MachineInstr *EndInstr = LIS->getInstructionFromIndex(End);
    GII->encodeBarrierMask(*EndInstr, PhysBarIdx);
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
    dbgs() << "(line " << LBR.beginIndex() << ")\n";
    LIS->getInstructionFromIndex(LBR.beginIndex())->dump();
    dbgs() << "(line " << LBR.endIndex() << ") ";
    LIS->getInstructionFromIndex(LBR.endIndex())->dump();
    dbgs() << " \t[PhysBarIdx: " << PhysBarIdx << ", " << getBTStr() << "]\n";
  }

  const std::vector<SlotIndex>& getStarts() const { return Starts; }
  SlotIndex getEnd() const { return End; }

private:
  BarrierType BT;
  // If this barrier is read barrier (for RAW)
  bool IsRAW = false;
  bool IsPhysical = false;
  unsigned PhysBarIdx = 0;
  std::vector<SlotIndex> Starts;
  SlotIndex End;
  LiveBarRange LBR;

  // Reserved physical barriers
  std::set<unsigned> Reserved;

  // helper function
  std::string const getBTStr() const {
  switch (BT) {
  default: llvm_unreachable("Invalid barrier type");
  case RAW_C : return "RAW_C";
  case RAW_S : return "RAW_S";
  case RAW_G : return "RAW_G";
  case RAW_TC : return "RAW_TC";
  case RAW_SFU : return "RAW_SFU";
  case WAR_G : return "WAR_G";
  case WAR_S : return "WAR_S";
  case WAR_MEM : return "WAR_MEM";
  }
  }
};

class LiveBarGraph {
  // TODO: should query ScheduleModel
  enum InterferenceType {
    IFTY_RAW_C,
    IFTY_RAW_S,
    IFTY_RAW_G,
    IFTY_RAW_L,
    IFTY_RAW, // General
    IFTY_WAR_S,
    IFTY_WAR_G,
    IFTY_WAR_L,
    IFTY_WAR, // General
    IFTY_DEFAULT
  };

  struct Edge {
    Barrier *Dst;
    unsigned Cost = 0;
    InterferenceType IFTY = IFTY_DEFAULT;
    // inteference type
    Edge(Barrier *Dst, unsigned Cost) : Dst(Dst), Cost(Cost) {}

    // Required by std::set
    bool operator<(const Edge& Other) const {
      return Dst < Other.Dst;
    }
  };

  std::vector<Barrier*> Nodes;
  std::map<Barrier*, std::set<Edge>> Edges;

  // Reserved physical barriers
  std::set<unsigned> Reserved;

  // Cache MBB, LIS
  const MachineBasicBlock &MBB;
  const LiveIntervals *LIS = nullptr;
public:
  // Build Interference graph with raw barriers
  LiveBarGraph(std::vector<Barrier> &Bars, std::set<unsigned> Reserved, 
               MachineBasicBlock &MBB, const LiveIntervals *LIS) 
               : Reserved(Reserved), MBB(MBB), LIS(LIS) {
    for (Barrier &CurBar : Bars) {
      for (Barrier* GraphNode : Nodes) {
        // Add interference edge
        if (CurBar.overlaps(*GraphNode)) {
          unsigned Cost = /* TODO */ 0;
          // Add new edge for current node
          if (Edges.find(&CurBar) == Edges.end())
            Edges[&CurBar] = std::set<Edge>{{GraphNode, Cost}};
          else
            Edges[&CurBar].emplace(GraphNode, Cost);

          // Add new edge for the other node
          if (Edges.find(GraphNode) == Edges.end())
            Edges[GraphNode] = std::set<Edge>{{&CurBar, Cost}};
          else 
            Edges[GraphNode].emplace(&CurBar, Cost);
        }
      }
      // Record node.
      Nodes.push_back(&CurBar);
    }
  }

  unsigned getMaxDegree() const {
    unsigned MaxDegree = 0;
    for (auto iter = Edges.begin(); iter != Edges.end(); ++iter) {
      MaxDegree = std::max(MaxDegree, unsigned(iter->second.size()));
    }
    return MaxDegree;
  }

  // Max number of activate barriers required at any instrs
  unsigned getMaxConcurrentBarriers() const {
    // Scan the MBB, count max concurrent barriers
    unsigned MaxConcurrentBarriers = 0;
    for (const MachineInstr &MI : MBB) {
      SlotIndex CurSI = LIS->getInstructionIndex(MI);
      unsigned CurrentMaxConBar = 0;
      for (Barrier *B : Nodes) {
        if (B->getLiveBarRange().liveAt(CurSI))
          CurrentMaxConBar++;
      }
      MaxConcurrentBarriers = std::max(MaxConcurrentBarriers, CurrentMaxConBar);
    }
    return MaxConcurrentBarriers;
  }

  void mergeBarriers() {
    // 1. find the barrier pair to merge
    Barrier *A = nullptr;
    Barrier *B = nullptr; // The node to be removed
    std::tie(A, B) = pickBarrierPairToMerge();

    LLVM_DEBUG(dbgs() << "To merge:\n");
    LLVM_DEBUG(A->dump());
    LLVM_DEBUG(B->dump());

    // 2. update the graph (a, b)
    //   a. delete note (A & B)
    Nodes.erase(std::remove(Nodes.begin(), Nodes.end(), A), Nodes.end());
    Nodes.erase(std::remove(Nodes.begin(), Nodes.end(), B), Nodes.end());

    //   b. delete edges (AB's neighbors)
    std::set<Barrier*> ABNeighbors;
    for (Edge const &e : Edges[A])
      if (e.Dst != B)
        ABNeighbors.insert(e.Dst);
    for (Edge const &e : Edges[B])
      if (e.Dst != A)
        ABNeighbors.insert(e.Dst);
    for (Barrier *Neighbor : ABNeighbors) {
      std::set<Edge> &NeighborsEdges = Edges[Neighbor];
      for (auto iter = NeighborsEdges.begin(); iter != NeighborsEdges.end(); ) {
        if (iter->Dst == A || iter->Dst == B)
          iter = NeighborsEdges.erase(iter);
        else 
          ++iter;
      }
    }
    Edges.erase(A);
    Edges.erase(B);

    // 3. merge!
    LLVM_DEBUG(dbgs() << "Merge two barriers\n");
    A->merge(*B);
    LLVM_DEBUG(dbgs() << "After merge: \n");
    LLVM_DEBUG(A->dump());
    // 3.1 Insert A
    Nodes.push_back(A);
    Edges[A];

    // 4. update edges
    // Add new edges that are linked to merged A
    // We only need to care about original neighbors
    for (Barrier *Neighbor : ABNeighbors) {
      if (!A->overlaps(*Neighbor)) 
        continue; // No longer overlaps

      unsigned Cost = 0; // TODO: update cost;

      Edges[A].emplace(Neighbor, Cost);

      if (Edges.find(Neighbor) == Edges.end())
        Edges[Neighbor] = std::set<Edge>{{A, Cost}};
      else
        Edges[Neighbor].emplace(A, Cost);
    }
  }

  // Main interface to choose barrier pair to merge
  std::pair<Barrier*, Barrier*> pickBarrierPairToMerge() {
    // We only merge barriers, whos degree is >= kNumBarriers
    std::set<Barrier*> Candidates;
    bool AreCandidatesValid = false;
    for (auto iter = Edges.begin(); iter != Edges.end(); ++iter) 
      if (iter->second.size() >= kNumBarriers - Reserved.size()) {
        AreCandidatesValid = true;
        Candidates.insert(iter->first);
        // Insert its neighbors
        for (const Edge &E : Edges[iter->first])
          Candidates.insert(E.Dst);
      }

    assert(AreCandidatesValid && "Candidates are invalid");
    // different strategies
    switch (PickBarPairAlg) {
    default: llvm_unreachable("Invalid algorithm");
    case PB_ALG_ORDERED:
      return pickBarrierPairToMergeOrdered(Candidates);
    case PB_ALG_ESTCOST:
      return pickBarrierPairToMergeEstCost(Candidates);
    case PB_ALG_MAXDEGREE:
      return pickBarrierPairToMergeMaxDegree(Candidates);
    }
  }

  // return an equivalent vector of barriers
  std::vector<Barrier> getResult() const {
    std::vector<Barrier> Result;
    for (Barrier *Bar : Nodes)
      Result.push_back(*Bar);
    return Result;
  }

  void dump() const {
    dbgs() << "************* LiveBarGraph **************\n";
    dbgs() << "Nodes: \n";
    for (Barrier *B : Nodes) 
      B->dump();
    dbgs() << "-----------------------------------------\n\n";
    dbgs() << "Edges: \n";
    for (auto iter : Edges) {
      // Src
      dbgs() << "-----------------------------------------\n";
      dbgs() << "#Dsts: " << iter.second.size() << "\n";
      iter.first->dump();
      dbgs() << "\n";
      // Dst
      for (const Edge &E : iter.second) {
        dbgs() << "-->";
        E.Dst->dump();
        dbgs() << "[Cost]: " << E.Cost << "\n";
      }
    }
  }

private:
  std::pair<Barrier*, Barrier*> 
  pickBarrierPairToMergeOrdered(std::set<Barrier*> &Candiates);
  std::pair<Barrier*, Barrier*> 
  pickBarrierPairToMergeEstCost(std::set<Barrier*> &Candidates);
  std::pair<Barrier*, Barrier*> 
  pickBarrierPairToMergeMaxDegree(std::set<Barrier*> &Candidates);
};
} // anonymous namespace

//=------------------------------------------------------------------------=//
// pick barrier pair algorithm
//=------------------------------------------------------------------------=//
// TODO: We need more theoretical support here.
std::pair<Barrier*, Barrier*> 
LiveBarGraph::pickBarrierPairToMergeOrdered(std::set<Barrier*> &Candidates) {
  // Both are RAW_TC
  for (Barrier *Candidate  : Candidates) {
    if (Candidate->getBarrierType() == RAW_TC) {
      for (Edge const &E : Edges[Candidate]) {
        if (E.Dst->getBarrierType() == RAW_TC)
          return {Candidate, E.Dst};
      }
    }
  }
  // Both are RAW_SFU
  for (Barrier *Candidate : Candidates) {
    if (Candidate->getBarrierType() == RAW_SFU) {
      for (Edge const &E : Edges[Candidate]) {
        if (E.Dst->getBarrierType() == RAW_SFU)
          return {Candidate, E.Dst};
      }
    }
  }
  // First merge LDC, then LDS, then LDG
  for (Barrier *Candidate : Candidates) {
    if (Candidate->getBarrierType() == RAW_C) {
      for (Edge const &E : Edges[Candidate]) {
        if (E.Dst->getBarrierType() == RAW_C)
          return {Candidate, E.Dst};
      }
    }
  }

  // No RAW_C interference, try LDS
  for (Barrier *Candidate : Candidates) {
    if (Candidate->getBarrierType() == RAW_S) {
      for (Edge const &E : Edges[Candidate]) {
        if (E.Dst->getBarrierType() == RAW_S)
          return {Candidate, E.Dst};
      }
    }
  }

  // try WAR_MEM
  for (Barrier *Candidate : Candidates) {
    if (Candidate->getBarrierType() == WAR_MEM) {
      for (Edge const &E : Edges[Candidate]) {
        if (E.Dst->getBarrierType() == WAR_MEM)
          return {Candidate, E.Dst};
      }
    }
  }

  // NO RAW_S interference, try LDG
  for (Barrier *Candidate : Candidates) {
    if (Candidate->getBarrierType() == RAW_G) {
      for (Edge const &E : Edges[Candidate]) {
        if (E.Dst->getBarrierType() == RAW_G)
          return {Candidate, E.Dst};
      }
    }
  }

  // else try to merge RAW (exclude RAW_G)
  for (Barrier *Candidate : Candidates)
    if (Candidate->isInOrderRAW())
      for (Edge const &E : Edges[Candidate])
        if (E.Dst->isInOrderRAW())
          return {Candidate, E.Dst};

  // Finally try to merge all RAWs
  for (Barrier *Candidate : Candidates) {
    if (Candidate->isRAW()) {
      for (Edge const &E : Edges[Candidate])
        if (E.Dst->isRAW()) 
          return {Candidate, E.Dst};
    }
  }

  llvm_unreachable("Should have returned");
}

std::pair<Barrier*, Barrier*> 
LiveBarGraph::pickBarrierPairToMergeEstCost(std::set<Barrier*> &Candiates) {
  llvm_unreachable("Not implemented");
}

std::pair<Barrier*, Barrier*> 
LiveBarGraph::pickBarrierPairToMergeMaxDegree(std::set<Barrier*> &Candidates) {
  llvm_unreachable("Not implemented");
}
// End of pick barrier pair alogrithm

namespace llvm {
  void initializeLiveIntervalsPass(PassRegistry&);
}

INITIALIZE_PASS_BEGIN(GASSBarrierSetting, DEBUG_TYPE,
                      GASS_BARRIERSETTING_NAME, false, false)
INITIALIZE_PASS_DEPENDENCY(LiveIntervals)
INITIALIZE_PASS_END(GASSBarrierSetting, DEBUG_TYPE,
                    GASS_BARRIERSETTING_NAME, false, false)

bool GASSBarrierSetting::runOnMachineFunction(MachineFunction &MF) {
  const GASSSubtarget &ST = MF.getSubtarget<GASSSubtarget>();
  GII = ST.getInstrInfo();
  GRI = ST.getRegisterInfo();
  MRI = &MF.getRegInfo();
  LIS = &getAnalysis<LiveIntervals>();

  // TODO: Should check if this function uses LDGDEPBAR & DEPBAR
  if (ST.getSmVersion() >= 80)
    Reserved.insert(0);

  // Use MachineRegisterInfo to get def-use chain
  // TODO: Get the LiveInterval
  for (MachineBasicBlock &MBB : MF) {
    runOnMachineBasicBlock(MBB);
  }

  return true;
}

/// If current MBB has only one successor & NOT ends with branch,
/// we also need to scan the next MBB
static std::vector<MachineInstr *> 
getScanRange(MachineBasicBlock &MBB, MachineBasicBlock::iterator iter) {
  std::vector<MachineInstr *> Res;

  // following instrs in the current MBB
  for (auto I = ++iter; I != MBB.end(); ++I) {
    Res.push_back(&*I);
    // If one MBB has two BRAs. The latter one is not in the ScanRange
    if (I == MBB.getFirstTerminator())
      break;
  }

  // Terminator MBB
  if (MBB.succ_size() == 0)
    return Res;

  // If MBB doesn't end with branch & only one successor, add instr in the succ
  if (!MBB.getLastNonDebugInstr()->isBranch()) {
    assert(MBB.succ_size() == 1);
    for (MachineInstr &I : **MBB.succ_begin()) {
      // TODO: change this to predicated instr
      if (I.isBranch())
        break; // Do not let BRA wait.
      Res.push_back(&I);
    }
  }
  return Res;
}

// One instruction may set multiple WAR barriers. We need to discard one, and
// keep the barrier that ends earlier
// TODO: Can one instruction set multiple RAW barriers?
static void removeDuplicatedWAR(std::vector<Barrier> &Bar) {
  std::map<SlotIndex, std::vector<Barrier*>> Idx2Barriers;
  for (Barrier &B : Bar) {
    if (!B.isRAW()) {
      assert(B.getStarts().size() == 1 && "We haven't merged barriers");
      SlotIndex StartIdx = B.getStarts()[0];
      Idx2Barriers[StartIdx].push_back(&B);
    }
  }

  // Remove duplicated
  std::vector<Barrier*> ToBeRemoved;
  for (auto const Idx2Bar : Idx2Barriers) {
    if (Idx2Bar.second.size() > 1) {
      assert(Idx2Bar.second.size() == 2);
      // Remove the barrier that ends latter
      if (Idx2Bar.second[0]->getEnd() < Idx2Bar.second[1]->getEnd())
        ToBeRemoved.push_back(Idx2Bar.second[1]);
      else
        ToBeRemoved.push_back(Idx2Bar.second[0]);
    }
  }
  std::vector<size_t> ToBeRemovedIdx;
  for (Barrier *Probe : ToBeRemoved) {
    for (size_t i = 0; i < Bar.size(); ++i) {
      if (Probe == &Bar[i]) {
        ToBeRemovedIdx.push_back(i);
        break;
      }
    }
  }
  std::sort(ToBeRemovedIdx.rbegin(), ToBeRemovedIdx.rend());
  for (size_t Probe : ToBeRemovedIdx)
    Bar.erase(Bar.begin() + Probe);

}

// One instruction may wait on multiple RAW barriers.
// If they are in-order barriers (all RAW barrier except LDG (LDL?)), we can
// coalesce them (only keep the barrier with the latest start time)
static void coalesceRAWs(std::vector<Barrier> &Bar) {
  // Ref: removeDuplicatedWAR
  size_t TotalCoalescedNum = 0;
  for (BarrierType ValidRAWMergeTy : {RAW_S, RAW_C, RAW_TC}) {
    std::map<SlotIndex, std::vector<Barrier*>> EndIdx2Barriers;
    for (Barrier &B : Bar) {
      if (B.getBarrierType() == ValidRAWMergeTy) {
        SlotIndex EndIdx = B.getEnd();
        EndIdx2Barriers[EndIdx].push_back(&B);
      }
    }

    // Coalesce RAWs that end with the same instr
    std::vector<Barrier*> ToBeRemoved;
    for (auto const &Idx2Bar : EndIdx2Barriers) {
      // If this instr waits on multiple RAW_S
      const std::vector<Barrier*> &CurBarriers = Idx2Bar.second;
      if (CurBarriers.size() > 1) {
        assert(CurBarriers[0]->getStarts().size() == 1 && 
              "We haven't merged barriers");
        SlotIndex LatestSlot = CurBarriers[0]->getStarts()[0];
        size_t LatestIdx = 0; 
        for (size_t i=1; i<CurBarriers.size(); ++i) {
          assert(CurBarriers[i]->getStarts().size() == 1 &&
                "We haven't merged barriers");
          if (CurBarriers[i]->getStarts()[0] > LatestSlot) {
            LatestSlot = CurBarriers[i]->getStarts()[0];
            LatestIdx = i;
          }
        }

        // Remove all barriers except the the latest one
        for (size_t i=0; i<CurBarriers.size(); ++i) {
          if (i != LatestIdx)
            ToBeRemoved.push_back(CurBarriers[i]);
        }
      }
    }
    std::vector<size_t> ToBeRemovedIdx;
    for (Barrier *Probe : ToBeRemoved) {
      for (size_t i=0; i<Bar.size(); ++i) {
        if (Probe == &Bar[i]) {
          ToBeRemovedIdx.push_back(i);
          break;
        }
      }
    }
    TotalCoalescedNum += ToBeRemovedIdx.size();
    // Remove from end to begin
    std::sort(ToBeRemovedIdx.rbegin(), ToBeRemovedIdx.rend());
    for (size_t Probe : ToBeRemovedIdx)
      Bar.erase(Bar.begin() + Probe);
  }
  // outs() << "Num coalesced barriers: " << TotalCoalescedNum << "\n";
}

void GASSBarrierSetting::runOnMachineBasicBlock(MachineBasicBlock &MBB) {
  CurMBB = &MBB;
  LLVM_DEBUG(MBB.dump());

  // Recored Barriers
  std::vector<Barrier> Barriers;

  // TODO: handle livein & liveout
  for (auto iter = MBB.begin(); iter != MBB.end(); ++iter) {
    MachineInstr &MI = *iter;

    // TODO: get rid of this.
    // Provide default flag encoding
    GII->initializeFlagsEncoding(MI);

    // Check RAW dependency
    if (GII->needsRAWBarrier(MI)) {
      // Check all defs (dsts) of load (requires RAW barriers)
      for (MachineOperand const &Def : MI.defs()) {
        if (Def.isReg()) {
          std::vector<MachineInstr *> ScanRange = getScanRange(MBB, iter);
          for (MachineInstr *probe : ScanRange) {
            // outs() << "Checking:\n";
            // probe->dump();
            for (MachineOperand const &Use : probe->explicit_uses()) {
              if ((Use.isReg() && GRI->regsOverlap(Use.getReg(), Def.getReg())) 
                  || probe == ScanRange.back()) {
                // The last instr needs to catch all dependency
                SlotIndex Start = LIS->getInstructionIndex(MI);
                SlotIndex End = LIS->getInstructionIndex(*probe);
                if (Start == End) {
                  outs() << "Start == End is incorrect.\n";
                  MI.dump();
                  probe->dump();
                  llvm_unreachable("Start == End is incorrect.");
                }
                // TODO: maybe we can record which operand to wait on.
                Barriers.emplace_back(MI, Start, End, false, LIS, GII);
                // Triple break
                goto CreateWARForMemOperand;
              }
            }
          } // We only care about reg
        }
      }
      CreateWARForMemOperand: {}
      // Create WAR for MemOperand
    } else if (GII->isStore(MI)) {
      // Create WAR for store instructions
      MachineOperand &MO = MI.getOperand(0);
      assert(MO.isReg() && "Expect Register from Store instr");

      // TODO: merge this with MemOperand WAR
      SlotIndex SIStart = LIS->getInstructionIndex(MI);

      // if current instr writes to it, we don't need to care about that
      std::vector<MachineInstr *> ScanRange = getScanRange(MBB, iter);
      for (MachineInstr *probe : ScanRange) {
        // The last inst must wait on this. (TODO: only if next BB writes to it)
        if (probe == ScanRange.back()) {
            SlotIndex SIEnd = LIS->getInstructionIndex(*probe);
            Barriers.emplace_back(MI, SIStart, SIEnd, true, LIS, GII);
            break;
        }
        for (MachineOperand &Def : probe->defs()) {
          if (Def.isReg() && GRI->regsOverlap(MO.getReg(), Def.getReg())) {
            SlotIndex SIEnd = LIS->getInstructionIndex(*probe);
            Barriers.emplace_back(MI, SIStart, SIEnd, true, LIS, GII);
            goto TheEndStoreWAR; // double break
          }
        }
      }
      TheEndStoreWAR: {}
    }

    // Check WAR dependency
    if (MachineOperand *BSrc = GII->getMemOperandReg(MI)) {
      SlotIndex SIStart = LIS->getInstructionIndex(MI);

      // if current instr writes to it, we don't need to care about that
      if (GII->isLDG(MI) || GII->isLDS(MI)) {
        Register Dst = MI.getOperand(0).getReg();
        Register Src = MI.getOperand(1).getReg();
        if (GRI->regsCover(Dst, Src, *MRI)) {
          continue;
        }
      } 
      // Record WAR barriers
      std::vector<MachineInstr *> ScanRange = getScanRange(MBB, iter);
      for (MachineInstr *probe : ScanRange) {
        // The last inst must wait on this.
        if (probe == ScanRange.back()) {
            SlotIndex SIEnd = LIS->getInstructionIndex(*probe);
            Barriers.emplace_back(MI, SIStart, SIEnd, true, LIS, GII);
            break;
        }
        for (MachineOperand &Def : probe->defs()) {
          if (Def.isReg() && GRI->regsOverlap(BSrc->getReg(), Def.getReg())) {
            SlotIndex SIEnd = LIS->getInstructionIndex(*probe);
            Barriers.emplace_back(MI, SIStart, SIEnd, true, LIS, GII);
            goto TheEnd; // double break
          }
        }
      }
      TheEnd: {}
    } // if (MachineOperand *BSrc = GII->getMemOperandReg(MI))

    // A special rule for the UR offset in LDSM_x4_rui
    if (MI.getOpcode() == GASS::LDSM_x4_rui) {
      MachineOperand *UROff = &MI.getOperand(2);
      SlotIndex SIStart = LIS->getInstructionIndex(MI);

      // Record WAR barriers
      std::vector<MachineInstr *> ScanRange = getScanRange(MBB, iter);
      for (MachineInstr *probe : ScanRange) {
        // The last inst must wait on this.
        if (probe == ScanRange.back()) {
            SlotIndex SIEnd = LIS->getInstructionIndex(*probe);
            Barriers.emplace_back(MI, SIStart, SIEnd, true, LIS, GII);
            break;
        }
        for (MachineOperand &Def : probe->defs()) {
          if (Def.isReg() && GRI->regsOverlap(UROff->getReg(), Def.getReg())) {
            SlotIndex SIEnd = LIS->getInstructionIndex(*probe);
            Barriers.emplace_back(MI, SIStart, SIEnd, true, LIS, GII);
            goto TheEnd2; // double break
          }
        }
      }
      TheEnd2: {}
    }

  }

  // 0.0 If multiple WAR barriers start with the same instruction, remove one
  removeDuplicatedWAR(Barriers);
  // 0.1 If multiple RAW ends with the same instruction, coalesce them
  coalesceRAWs(Barriers);
  
  // 1. Build interference graph
  LiveBarGraph TheGraph(Barriers, Reserved, MBB, LIS);
  LLVM_DEBUG(dbgs() << "*** Build Barrier interference graph ***\n");
  LLVM_DEBUG(TheGraph.dump());

  // 2. Merge if necessary
  while (TheGraph.getMaxConcurrentBarriers() > kNumBarriers - Reserved.size())
    TheGraph.mergeBarriers();
  LLVM_DEBUG(dbgs() << "** After merging **\n");
  LLVM_DEBUG(TheGraph.dump());
  // 3. Revert Graph back to linear barriers
  std::vector<Barrier> NewBarriers = TheGraph.getResult();

  // 3. Allocate physical Barriers
  allocatePhysBarriers(NewBarriers);

  // 3.1 print allocated result (debug info)
  for (const Barrier &Bar : NewBarriers) {
    LLVM_DEBUG(Bar.dump());
  }

  // 4. Encoding Barrier info
  for (const Barrier &Bar : NewBarriers)
    Bar.encodeBarrierInfo(GII);
}

//=-----------------------------------------------=//
// Private helper functions
//=-----------------------------------------------=//
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

void GASSBarrierSetting::allocatePhysBarriers(std::vector<Barrier> &Barriers) {
  std::set<unsigned> FreePhysBar;
  for (unsigned i=0; i<kNumBarriers; ++i)
    if (Reserved.find(i) == Reserved.end()) // Skip reserved barriers
      FreePhysBar.insert(i);

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
    bool HasRAW = false;
    bool HasWAR = false;
    for (auto it = RemainingBarriers.begin(); it != RemainingBarriers.end(); ) {
      // Now we meet a barrier that starts at CurSI
      if ((*it)->getLiveBarRange().liveAt(CurSI)) {
        // Make sure that one instruction can not set more than one RAW/WAR
        if ((*it)->isRAW()) {
          assert(HasRAW == false);
          HasRAW = true;
        } else {
          assert(HasWAR == false);
          HasWAR = true;
        }
        // assert(!FreePhysBar.empty() && "No free physical barrier");
        if (FreePhysBar.empty()) {
          dbgs() << "Active Barriers:\n";
          for (auto const &AB : ActiveBarriers)
            AB.first->dump();
          dbgs() << "Current barrier:\n";
          (*it)->dump();
          llvm_unreachable("No free physical barrier");
        }
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
