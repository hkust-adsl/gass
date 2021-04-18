#include "GASSSchedStrategy.h"
#include "GASSRegisterInfo.h"
#include "GASSInstrInfo.h"
#include "MCTargetDesc/GASSMCTargetDesc.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineScheduler.h"
#include "llvm/CodeGen/RegisterClassInfo.h"
#include "llvm/CodeGen/ScheduleDAG.h"
#include "llvm/MC/MCSchedule.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include <algorithm>
#include <queue>

using namespace llvm;

#define DEBUG_TYPE "gass-machine-scheduler"

//==------------------------------------------------------------------------==//
// ScoreBoard
GASSScoreBoard::GASSScoreBoard() {}
// End of ScoreBoard
//==------------------------------------------------------------------------==//

SUnit *GASSSchedStrategy::pickNode(bool &IsTopNode) {
  // Use default strategy for non-loop-body MBBs
  if (!IsLoopBody)
    return GenericScheduler::pickNode(IsTopNode);

  // Mostly copy-and-paste from GenericScheduler::pickNode()
  // We need to copy them here because GenericScheduler::pickNodeFromQueue() is
  // not virtual.
  if (DAG->top() == DAG->bottom()) {
    assert(Top.Available.empty() && Top.Pending.empty() &&
           Bot.Available.empty() && Bot.Pending.empty() && "ReadyQ garbage");
    return nullptr;
  }
  SUnit *SU;
  do {
    if (RegionPolicy.OnlyTopDown) {
      if (!IsLoopBody)
        SU = Top.pickOnlyChoice();
      else 
        SU = pickOnlyChoice(Top);
      if (SU != nullptr && IsLoopBody) {
        LLVM_DEBUG(dbgs() << "\n** Pick the only candidate: ");
        LLVM_DEBUG(DAG->dumpNodeName(*SU));
        LLVM_DEBUG(dbgs() << "\nWhile the pending queue is:\n");
        LLVM_DEBUG(Top.Pending.dump());
        LLVM_DEBUG(SU->getInstr()->dump());
      }
      if (!SU) {
        CandPolicy NoPolicy;
        TopCand.reset(NoPolicy);
        pickNodeFromQueue(Top, NoPolicy, DAG->getTopRPTracker(), TopCand);
        assert(TopCand.Reason != NoCand && "failed to find a candidate");
        SU = TopCand.SU;
      }
      IsTopNode = true;
    } else if (RegionPolicy.OnlyBottomUp) {
      SU = Bot.pickOnlyChoice();
      if (!SU) {
        CandPolicy NoPolicy;
        BotCand.reset(NoPolicy);
        pickNodeFromQueue(Bot, NoPolicy, DAG->getBotRPTracker(), BotCand);
        assert(BotCand.Reason != NoCand && "failed to find a candidate");
        SU = BotCand.SU;
      }
      IsTopNode = false;
    } else {
      SU = pickNodeBidirectional(IsTopNode);
    }
  } while (SU->isScheduled);

  if (SU->isTopReady())
    Top.removeReady(SU);
  if (SU->isBottomReady())
    Bot.removeReady(SU);

  LLVM_DEBUG(dbgs() << "Scheduling SU(" << SU->NodeNum << ") "
                    << *SU->getInstr());
  return SU;
}

SUnit *GASSSchedStrategy::pickOnlyChoice(SchedBoundary &Zone) {
  assert(Zone.isTop() && "Only support top-down");

  // if (Zone.CheckPending)
  // Since CheckPending is private, then we always releasePending()
  Zone.releasePending();

  // Defer any ready instrs that now have a hazard.
  for (ReadyQueue::iterator I = Zone.Available.begin(); 
                            I != Zone.Available.end();) {
    if (Zone.checkHazard(*I)) {
      Zone.Pending.push(*I);
      I = Zone.Available.remove(I);
      continue;
    }
    ++I;
  }

  // Add more as available
  while (Zone.Available.size() <= 2) {
    Zone.bumpCycle(Zone.getCurrCycle() + 1);
    Zone.releasePending();
    if (Zone.Pending.empty())
      break;
  }

  LLVM_DEBUG(Zone.Pending.dump());
  LLVM_DEBUG(Zone.Available.dump());

  if (Zone.Available.size() == 1)
    return *Zone.Available.begin();
  return nullptr;
}

void GASSSchedStrategy::pickNodeFromQueue(SchedBoundary &Zone,
                                         const CandPolicy &ZonePolicy,
                                         const RegPressureTracker &RPTracker,
                                         SchedCandidate &Cand) {
  ReadyQueue &Q = Zone.Available;
  // sort the Q based on scores
  std::vector<int> Scores(Q.size());
  for (size_t i=0; i<Q.size(); ++i)
    Scores[i] = computeScore(Q.elements()[i]);
  if (IsLoopBody) {
    // GASS-specific heuristic
    if (tryPickNodeFromQueue(Zone, ZonePolicy, Cand))
      return;
    // fall-throught default heuristic
    llvm_unreachable("Should have returned");
  }
  for (SUnit *SU : Q) {
    SchedCandidate TryCand(ZonePolicy);
    initCandidate(TryCand, SU, Zone.isTop(), RPTracker, TRI);
    // Pass SchedBoundary only when comparing nodes from the same boundary.
    SchedBoundary *ZoneArg = Cand.AtTop == TryCand.AtTop ? &Zone : nullptr;
    tryCandidate(Cand, TryCand, ZoneArg);
    if (TryCand.Reason != NoCand) {
      // Initialize resource delta if needed in case future heuristics query it.
      if (TryCand.ResDelta == SchedResourceDelta())
        TryCand.initResourceDelta(DAG, SchedModel);
      Cand.setBest(TryCand);
      LLVM_DEBUG(traceCandidate(Cand));
    }
  }
}

//==------------------------------------------------------------------------==//
// GASS heuristic
//==------------------------------------------------------------------------==//
std::vector<int> GASSSchedStrategy::getSUScore(SUnit *SU) {
  std::vector<int> Score(SCHED_PRIORITY_SIZE, 0);
  // bias COPY (following the practice in GenericScheduler)
  computeCOPYScore(Score, SU);
  computeLDGScore(Score, SU);
  computeLDSScore(Score, SU);
  computeFreeResourceScore(Score, SU);
  // bias instr that reads 
  computeCarryInScore(Score, SU);
  computeLatencyStallScore(Score, SU);
  // Fall through to order
  computeOrderScore(Score, SU);

  return Score;
}

bool GASSSchedStrategy::isOkToIssueLDG() const {
  // TODO: implement this.
  return false;
  // return true;
}

// Limit the number of live registers brought by LDSs
bool GASSSchedStrategy::isOkToIssueLDS() const {
  if (ScoreBoard.ActiveLDSs.size() <= 4)
    return true;
  else
    return false;
}

void GASSSchedStrategy::computeCOPYScore(std::vector<int> &Score, SUnit *SU) {
  // TODO: fill this
  Score[SCHED_COPY] = 0;
  return;
}

void GASSSchedStrategy::computeLDGScore(std::vector<int> &Score, SUnit *SU) {
  if (GASSInstrInfo::isLDG(*SU->getInstr())) {
    if (!isOkToIssueLDG())
      // Don't overwhelm LDST
      return;
    Score[SCHED_LDG] = 1;
  } else // do nothing
    return;
}

void GASSSchedStrategy::computeLDSScore(std::vector<int> &Score, SUnit *SU) {
  if (GASSInstrInfo::isLDS(*SU->getInstr())) {
    if (!isOkToIssueLDS())
      // Don't overwhelm LDST
      return;
    Score[SCHED_LDS] = 1; 
  } else // do nothing
    return;
}

void GASSSchedStrategy::computeFreeResourceScore(std::vector<int> &Score, 
                                                 SUnit *SU) {
  // assert(IsTopNode)?
  // Check is all required resources are free
  const MCSchedClassDesc *SC = DAG->getSchedClass(SU);
  for (const MCWriteProcResEntry &PE :
       make_range(SchedModel->getWriteProcResBegin(SC),
                  SchedModel->getWriteProcResEnd(SC))) {
    unsigned ReservedUntil, InstanceIdx;
    std::tie(ReservedUntil, InstanceIdx) = 
        Top.getNextResourceCycle(PE.ProcResourceIdx, 0);
    if (ReservedUntil >= Top.getCurrCycle())
      // Resource still busy
      return;
  }
  // All required resources are free
  Score[SCHED_FREE_RESOURCE] = 1;

  // Check if it is LDS
  if (GASSInstrInfo::isLDS(*SU->getInstr()))
    if (!isOkToIssueLDS())
      Score[SCHED_FREE_RESOURCE] = 0;
}

void GASSSchedStrategy::computeCarryInScore(std::vector<int> &Score, 
                                            SUnit* SU) {
  const MachineInstr *Instr = SU->getInstr();
  if (GASSInstrInfo::ifReadsCarryIn(*Instr))
    Score[SCHED_CARRYIN] = 1;
}

// TODO: better to name "LatencyStall" as "WaitCycle"?
void GASSSchedStrategy::computeLatencyStallScore(std::vector<int> &Score, 
                                                 SUnit *SU) {
  int StallLatency = Top.getCurrCycle() - SU->TopReadyCycle;
  assert(StallLatency >= 0);
  // Force this number to be 0 for LDS
  if (GASSInstrInfo::isLDS(*SU->getInstr()))
    StallLatency = 0;
  Score[SCHED_LATENCY] = StallLatency;
}

void GASSSchedStrategy::computeOrderScore(std::vector<int> &Score, SUnit *SU) {
  Score[SCHED_ORDER] = -SU->NodeNum;
}
//------- End computeXXXScore()

bool GASSSchedStrategy::tryPickNodeFromQueue(SchedBoundary &Zone,
                                             const CandPolicy &ZonePolicy,
                                             SchedCandidate &Cand) {
  ReadyQueue &Q = Zone.Available;
  std::vector<std::vector<int>> Scores;
  for (SUnit *SU : Q) {
    std::vector<int> Score = getSUScore(SU);
    Scores.push_back(Score);
  }

  LLVM_DEBUG(dbgs() << "** GASSSchedStrategy::tryPickNodeFromQueue **\n");
  LLVM_DEBUG(Top.Available.dump());
  LLVM_DEBUG(Top.Pending.dump());

  // dump remain resources
  LLVM_DEBUG(dbgs() << "** Remaining Res Count **\n");
  for (unsigned PIdx = 1; PIdx != SchedModel->getNumProcResourceKinds(); ++PIdx) {
    LLVM_DEBUG(
      dbgs() << SchedModel->getResourceName(PIdx) << " : "
             << Top.Rem->RemainingCounts[PIdx] << "\n");
  }
  LLVM_DEBUG(dbgs() << "** Resource reserved result ** (crrent cycle:" 
                    << Top.getCurrCycle() << ")\n");
  for (unsigned PIdx = 1; PIdx != SchedModel->getNumProcResourceKinds(); ++PIdx) {
    LLVM_DEBUG(
      dbgs() << SchedModel->getResourceName(PIdx) << " : "
             << Top.getNextResourceCycle(PIdx, 0).first << "\n");
  }

  // pick candidate with highest score
  size_t MaxIdx = std::distance(Scores.begin(), 
                                std::max_element(Scores.begin(), Scores.end()));
  
  LLVM_DEBUG(dbgs() << "Current best candidate is:\n");
  LLVM_DEBUG(Q.elements()[MaxIdx]->getInstr()->dump());
  LLVM_DEBUG(DAG->dumpNodeName(*Q.elements()[MaxIdx]));
  
  LLVM_DEBUG(dbgs() << ", score of which is:\n");
  LLVM_DEBUG(dbgs() << "{");
  for (int i=0; i<SCHED_PRIORITY_SIZE; ++i)
    LLVM_DEBUG(dbgs() << Scores[MaxIdx][i] << ", ");
  LLVM_DEBUG(dbgs() << "}\n");

  // dump #activeLDS
  if (GASSInstrInfo::isLDS(*Q.elements()[MaxIdx]->getInstr())) {
    LLVM_DEBUG(dbgs() << "  Num of active LDS: " 
                      << ScoreBoard.ActiveLDSs.size() << "\n");
  }

  LLVM_DEBUG(dbgs() << "\n");

  // TODO: RPDelta?
  SchedCandidate BestCand;
  BestCand.AtTop = true;
  BestCand.Reason = NodeOrder; // Doesn't mean any thing
  BestCand.SU = Q.elements()[MaxIdx];
  Cand.setBest(BestCand);

  return true;
}

// TODO: VReg1?
void GASSSchedStrategy::initCandidate(
    SchedCandidate &Cand, SUnit *SU, bool AtTop, 
    const RegPressureTracker &RPTracker, const TargetRegisterInfo *TRI) {
  Cand.SU = SU;
  Cand.AtTop = AtTop;

  RegPressureTracker &TempTracker = const_cast<RegPressureTracker&>(RPTracker);

  Pressure.clear();
  MaxPressure.clear();

  if (AtTop)
    TempTracker.getDownwardPressure(SU->getInstr(), Pressure, MaxPressure);
  else
    TempTracker.getUpwardPressure(SU->getInstr(), Pressure, MaxPressure);
  
  unsigned VReg1Pressure = Pressure[GASS::RegisterPressureSets::VReg1];
  unsigned VReg32Pressure = Pressure[GASS::RegisterPressureSets::VReg32];

  // We only need to update the RPDelta.Excess for instructions that increase
  // register pressure
  if (VReg32Pressure >= VReg32ExcessLimit) {
    Cand.RPDelta.Excess = PressureChange(GASS::RegisterPressureSets::VReg32);
    Cand.RPDelta.Excess.setUnitInc(VReg32Pressure - VReg32ExcessLimit);
  }

  // Register pressure is considiered 'CRITICAL' if it would make the following
  // basic block spills.
  int VReg32Delta = VReg32Pressure - VReg32CriticalLimit;

  if (VReg32Delta >= 0) {
    Cand.RPDelta.CriticalMax = 
        PressureChange(GASS::RegisterPressureSets::VReg32);
    Cand.RPDelta.CriticalMax.setUnitInc(VReg32Delta);
  }
}

//------------------------------------------------------------------------------
/// Apply a set of heuristics to a new candidate. Heuristics are currently
/// hierarchical. This may be more efficient than a graduated cost model because
/// we don't need to evaluate all aspects of the model for each node in the
/// queue. But it's really done to make the heuristics easier to debug and
/// statistically analyze.
///
/// \param Cand provides the policy and current best candidate.
/// \param TryCand refers to the next SUnit candidate, otherwise uninitialized.
/// \param Zone describes the scheduled zone that we are extending, or nullptr
//             if Cand is from a different zone than TryCand.
void GASSSchedStrategy::tryCandidate(SchedCandidate &Cand, 
                                     SchedCandidate &TryCand,
                                     SchedBoundary *Zone) const {
  assert(!IsLoopBody);
  if (!IsLoopBody)
    return GenericScheduler::tryCandidate(Cand, TryCand, Zone);

  //---------- GASS-specific heuristic for the main loop -------------
  // Initialize the candidate if needed.
  // if (!Cand.isValid()) {
  //   TryCand.Reason = NodeOrder;
  //   return;
  // }

  // assert(Zone != nullptr && "GASSSchedStrategy focus on top-down now");
  // assert(DAG->isTrackingPressure() && "Should track pressure");

  // // Assume the current loop is compute-bound
  // // 1. bias LDGs
  // if (tryLDG(TryCand, Cand, Zone))
  //   return;

  // // 2. bias LDSs
  // if (tryLDS(TryCand, Cand, Zone))
  //   return;

  // // 3. Do we have any free resources?
  // if (tryFreeResources(TryCand, Cand, Zone))
  //   return;

  // // 4. Prioritize instructions that read unbuffered resources by stall cycles
  // // TODO: bias critical path
  // if (tryLess(Zone->getLatencyStallCycles(TryCand.SU),
  //             Zone->getLatencyStallCycles(Cand.SU), TryCand, Cand, Stall))
  //   return;

  // // 5. (?) For loops that are acyclic path limitied, aggressively schedule
  // //    for latency.
  // if (tryLatency(TryCand, Cand, *Zone))
  //   return;

  // // 6. (?) Avoid increasing the max pressure of the entire region
  // if (tryPressure(TryCand.RPDelta.CurrentMax, 
  //                 Cand.RPDelta.CurrentMax,
  //                 TryCand, Cand, RegMax, TRI, DAG->MF))
  //   return;

  // // 7. (?) Avoid critical resource consumption and balance the schedule.
  // TryCand.initResourceDelta(DAG, SchedModel);
  // if (tryLess(TryCand.ResDelta.CritResources, Cand.ResDelta.CritResources,
  //             TryCand, Cand, ResourceReduce))
  //   return;
  // if (tryGreater(TryCand.ResDelta.DemandedResources,
  //                Cand.ResDelta.DemandedResources,
  //                TryCand, Cand, ResourceDemand))
  //   return;

  // // 8. Fall through to original instruction order.
  // // TODO: or to GenericScheduler::tryCandidate()?
  // if ((Zone->isTop() && TryCand.SU->NodeNum < Cand.SU->NodeNum)) {
  //   TryCand.Reason = NodeOrder;
  // }
}

/// Update info after schedule. Callback after a node is scheduled.
void GASSSchedStrategy::schedNode(SUnit *SU, bool IsTopNode) {
  if (IsTopNode) {
    SU->TopReadyCycle = std::max(SU->TopReadyCycle, Top.getCurrCycle());
    Top.bumpNode(SU);
    if (SU->hasPhysRegUses)
      reschedulePhysReg(SU, true);
    // Update Score board
    if (IsLoopBody) {
      LLVM_DEBUG(dbgs() << "  schedNode() -> ");
      LLVM_DEBUG(SU->getInstr()->dump());
      LLVM_DEBUG(dbgs() << "\n");
      if (GASSInstrInfo::isLDS(*SU->getInstr())) {
        ScoreBoard.ActiveLDSs.insert(SU);
        LLVM_DEBUG(dbgs() << "Insert active LDS:\n");
        LLVM_DEBUG(DAG->dumpNodeName(*SU));
        LLVM_DEBUG(dbgs() << "ActiveLDSs.size() == " 
                          << ScoreBoard.ActiveLDSs.size() << "\n");
      } else if (GASSInstrInfo::isLDG(*SU->getInstr())) {
        ScoreBoard.ActiveLDGs.insert(SU);
      }

      // expire old LDG & LDS
      for (const SDep &Pred : SU->Preds) {
        // Only consider data-dependency
        for (const SDep &Pred : SU->Preds) {
          if (Pred.getKind() == SDep::Data) {
            // expire old LDGs
            for (auto iter = ScoreBoard.ActiveLDGs.begin(); 
                      iter != ScoreBoard.ActiveLDGs.end(); ) {
              if (*iter == Pred.getSUnit())
                iter = ScoreBoard.ActiveLDGs.erase(iter);
              else
                ++iter;
            }
          }
        }
      }
      // expire old LDSs
      // Note: an LDS is considered to be expired when all successors are
      // scheduled.
      for (auto iter = ScoreBoard.ActiveLDSs.begin(); 
                iter != ScoreBoard.ActiveLDSs.end(); ) {
        // if ((*iter)->NumSuccsLeft == 0)
        if (std::all_of((*iter)->Succs.begin(), (*iter)->Succs.end(),
                        [](SDep &SD) -> bool { 
                          if (SD.isBarrier()) return true; // ignore barrier
                          return SD.getSUnit()->isScheduled; 
                        })) {
          LLVM_DEBUG(dbgs() << "Expire LDS:\n");
          LLVM_DEBUG(DAG->dumpNodeName(**iter));
          LLVM_DEBUG(dbgs() << "\n");
          iter = ScoreBoard.ActiveLDSs.erase(iter);
          LLVM_DEBUG(dbgs() << "ActiveLDSs.size() == " 
                            << ScoreBoard.ActiveLDSs.size() << "\n");
        } else
          ++iter;
      }
    }
  } else {
    assert(!IsLoopBody);
    SU->BotReadyCycle = std::max(SU->BotReadyCycle, Bot.getCurrCycle());
    Bot.bumpNode(SU);
    if (SU->hasPhysRegDefs)
      reschedulePhysReg(SU, false);
  }
}

// If this MBB is inside a loop.
// Focus on top-down now.
void GASSSchedStrategy::enterMBB(MachineBasicBlock *MBB) {
  IsLoopBody = false;
  // The default policy 
  RegionPolicy.OnlyBottomUp = false;
  RegionPolicy.OnlyTopDown = false;
  // We don't consider nested loop
  if (MLI->getLoopDepth(MBB) == 1) {
    IsLoopBody = true;
    RegionPolicy.OnlyBottomUp = false;
    RegionPolicy.OnlyTopDown = true;
  }
}

// print debug info & set Excess, Critical limits
void GASSSchedStrategy::initialize(ScheduleDAGMI *dag) {
  GenericScheduler::initialize(dag);
  // TODO: move this to class XXX : public ScheduleDAGMILive {}
  // // also print debug info
  // if (IsLoopBody)
  //   DAG->RPTracker.dump();
  LLVM_DEBUG(DAG->dump());

  // Set limit
  const int ErrorMargin = 3;
  VReg1ExcessLimit = Context->RegClassInfo->
      getNumAllocatableRegs(&GASS::VReg1RegClass);
  VReg32ExcessLimit = Context->RegClassInfo->
      getNumAllocatableRegs(&GASS::VReg32RegClass) - ErrorMargin;
  
  // TODO: revisit this.
  VReg32CriticalLimit = TRI->getRegPressureSetLimit(DAG->MF, 
    GASS::RegisterPressureSets::VReg32);
  VReg32CriticalLimit -= ErrorMargin;
}

// Called by ScheduleDAGMI::initQueues(TopRoots, BotRoots);
void GASSSchedStrategy::registerRoots() {
  // base registerRoots() updates CriticalPath
  GenericScheduler::registerRoots();

  // collect pipeline info
  const TargetSubtargetInfo &STI = DAG->MF.getSubtarget();
  DenseMap<InstrStage::FuncUnits, unsigned> Resources;

  for (const SUnit &SU : DAG->SUnits) {
    // Ref: MachinePipeliner::calcCriticalResources
    unsigned SchedClass = SU.getInstr()->getDesc().getSchedClass();
    const MCSchedClassDesc *SCDesc = 
        STI.getSchedModel().getSchedClassDesc(SchedClass);

    for (const MCWriteProcResEntry &PRE :
         make_range(STI.getWriteProcResBegin(SCDesc),
                    STI.getWriteProcResEnd(SCDesc))) {
      assert(PRE.Cycles != 0);
      Resources[PRE.ProcResourceIdx] += PRE.Cycles;
    }
  }

  // dump pipeline info
  // if (!DAG->SUnits.empty()) {
  //   outs() << "In MBB " << DAG->SUnits[0].getInstr()->getParent()->getName() << ":\n";
  //   for (auto iter : Resources) {
  //     const MCProcResourceDesc * ProcResDesc 
  //         = STI.getSchedModel().getProcResource(iter.first);
  //     outs() << ProcResDesc->Name << ":" << iter.second << "\n";
  //   }
  //   outs() << "\n";
  // }
}

int GASSSchedStrategy::computeScore(SUnit *SU) {
  // compute the priority of a SU. High score means high priority.
  int CriticalPathScore = 1;
  int ResourceScore = 1;
  int WaitingScore = 1;
  // TODO: Tune them
  int CriticalPathFactor = 1;
  int ResourceFactor = 1;
  int WaitingFactor = 1;

  // Compute CriticalPathScore
  //  try to schedule nodes on the critical path first
  {
    //
  }

  // Compute ResourceScore
  {
    const MCSchedClassDesc *SC = DAG->getSchedClass(SU);
    for (TargetSchedModel::ProcResIter
           PI = SchedModel->getWriteProcResBegin(SC),
           PE = SchedModel->getWriteProcResEnd(SC); PI != PE; ++PI) {
      unsigned PIdx = PI->ProcResourceIdx;
      unsigned PCycles = PI->Cycles;
      // Is the source free?
      unsigned NRCycle, InstanceIdx;
      std::tie(NRCycle, InstanceIdx) = Bot.getNextResourceCycle(PIdx, PCycles);
      // Resource not available, ResourceScore = 0;
      if (NRCycle > Bot.getCurrCycle()) {
        ResourceScore = 0;
        break;
      }
    }
  }

  // Compute WaitingScore
  //  try not to let a ready node to wait for too long
  {
    // outs() << "Ready Cycle: " << SU->BotReadyCycle << "\n"
    //        << "Current Cycle: " << Bot.getCurrCycle() << "\n"
    //        << "Latency: " << SU->Latency << "\n\n";
  }

  return CriticalPathScore * CriticalPathFactor +
         ResourceScore * ResourceFactor +
         WaitingScore * WaitingFactor;
}