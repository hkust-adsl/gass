// Ref: AMDGPU/GCNSchedStrategy.h

#ifndef LLVM_LIB_TARGET_GASS_GASSSCHEDSTRATEGY_H
#define LLVM_LIB_TARGET_GASS_GASSSCHEDSTRATEGY_H

#include "llvm/CodeGen/MachineScheduler.h"

namespace llvm {
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
public:
  GASSSchedStrategy(const MachineSchedContext *C)
    : GenericScheduler(C), MLI(C->MLI) {}
    
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
protected:
  void tryCandidate(SchedCandidate &Cand, SchedCandidate &TryCand,
                    SchedBoundary *Zone) const override;
private:
  int computeScore(SUnit *SU);
};

// class GASSScheduleDAGMILive final : public ScheduleDAGMILive {

// };
} // namespace llvm

#endif