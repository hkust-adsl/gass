// Ref: AMDGPU/GCNSchedStrategy.h

#ifndef LLVM_LIB_TARGET_GASS_GASSSCHEDSTRATEGY_H
#define LLVM_LIB_TARGET_GASS_GASSSCHEDSTRATEGY_H

#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/ScheduleDAG.h"

#include <set>

namespace llvm {
/// This is a complement for SchedBoundary.
struct GASSScoreBoard {
  // Active LDST instructions in the fly
  std::set<SUnit*> ActiveLDGs;
  std::set<SUnit*> ActiveLDSs;

public:
  explicit GASSScoreBoard();
}; // class GASSScoreBoard

class GASSSchedStrategy final : public GenericScheduler {
  const MachineLoopInfo *MLI = nullptr;

  /// Should we use special schedule strategy in this MBB?
  bool IsLoopBody = false;

  /// Ref: GCNSchedStrategy.h
  /// initCandidate() updates candidate's RPDelta
  void initCandidate(SchedCandidate &Cand, SUnit *SU,
                     bool AtTop, const RegPressureTracker &RPTracker,
                     const TargetRegisterInfo *TRI);

  // TODO: why we need both Pressure and MaxPressure?
  std::vector<unsigned> Pressure;
  std::vector<unsigned> MaxPressure;

  unsigned VReg1ExcessLimit = 0;
  unsigned VReg32ExcessLimit = 0;
  // unsigned SReg32ExcessLimit;

  unsigned VReg1CriticalLimit = 0;
  unsigned VReg32CriticalLimit = 0;

  // Caches ScoreBoard
  GASSScoreBoard ScoreBoard;

  // Critial Resource
  InstrStage::FuncUnits CritialFU;
  unsigned MaxCycles = 0;

  // Record k of instructions of interest
  DenseMap<SUnit*, int> Ks;
  DenseMap<int, DenseSet<SUnit*>> RemMaths;
  int CurrentK = 0;
  // Record depdencies of the first group of LDSs
  DenseSet<SUnit*> LdsDeps;
  // Plans for LDGs
  int MaxLDGsPerK = 0;
  int IssuedLDGs = 0;

  /// higher means higher priority
  /// Samiliar to CandReason in GenericSchedulerBase
  enum SchedPriority : uint8_t {
    // SCHED_COPY = 0,
    SCHED_MATH = 0,
    SCHED_LDS, 
    SCHED_LDG,
    SCHED_LDS_IDX,
    SCHED_FREE_RESOURCE,
    // SCHED_CARRYIN,
    // SCHED_LATENCY,
    SCHED_ORDER,
    // Record size of all reasons
    SCHED_PRIORITY_SIZE
  };
public:
  GASSSchedStrategy(const MachineSchedContext *C)
    : GenericScheduler(C), MLI(C->MLI), ScoreBoard() {}
    
  void registerRoots() override;

  void enterMBB(MachineBasicBlock *MBB) override;
  // void leaveMBB(MachineBasicBlock *MBB) override;

  SUnit *pickNode(bool &IsTopNode) override;

  void pickNodeFromQueue(SchedBoundary &Zone,
                         const CandPolicy &ZonePolicy,
                         const RegPressureTracker &RPTracker,
                         SchedCandidate &Candidate);

  // Print debug info 
  void initialize(ScheduleDAGMI *dag) override;

  // // Only bottom-up for GASS
  // void initPolicy(MachineBasicBlock::iterator Begin,
  //                 MachineBasicBlock::iterator End,
  //                 unsigned NumRegionInstrs) override;

  void schedNode(SUnit *SU, bool IsTopNode) override;
protected:
  void tryCandidate(SchedCandidate &Cand, SchedCandidate &TryCand,
                    SchedBoundary *Zone) const override;
private:
  // interfaces for GASS-specific heuristic
  /// computes schedule score (priority) of a node
  std::vector<int> getSUScore(SUnit *SU);
  void computeCOPYScore(std::vector<int> &Score, SUnit *SU);
  void computeMathScore(std::vector<int> &Score, SUnit *SU);
  void computeLDSScore(std::vector<int> &Score, SUnit *SU);
  void computeLDGScore(std::vector<int> &Score, SUnit *SU);
  void computeLDSIdxScore(std::vector<int> &Score, SUnit *SU);  
  void computeFreeResourceScore(std::vector<int> &Score, SUnit *SU);
  // void computeCarryInScore(std::vector<int> &Score, SUnit *SU);
  // void computeLatencyStallScore(std::vector<int> &Score, SUnit *SU);
  void computeOrderScore(std::vector<int> &Score, SUnit *SU);
  void constructKs();

  /// If it is ok to issue ldg now. (try to interleave LDGs)
  bool isOkToIssueLDS(SUnit *SU);
  bool isResourceFree(SUnit *SU);
  bool isCritialResourceRequired(SUnit *SU);
  bool isMathSU(SUnit *SU);

  /// returns true if new candidate is found
  bool tryPickNodeFromQueue(SchedBoundary &Zone, const CandPolicy &ZonePolicy,
                            SchedCandidate &Cand);

  /// simulate SchedBoundary::pickOnlyChoice()
  SUnit *pickOnlyChoice(SchedBoundary &Zone);

public:
  friend class SchedBoundary;
};

// class GASSScheduleDAGMILive final : public ScheduleDAGMILive {

// };
} // namespace llvm

#endif