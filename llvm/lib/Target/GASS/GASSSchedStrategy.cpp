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
      SU = pickOnlyChoice(Top);
      // if (SU != nullptr && IsLoopBody) {
      //   LLVM_DEBUG(dbgs() << "\n** Pick the only candidate: ");
      //   LLVM_DEBUG(DAG->dumpNodeName(*SU));
      //   LLVM_DEBUG(dbgs() << "\nWhile the pending queue is:\n");
      //   LLVM_DEBUG(Top.Pending.dump());
      //   LLVM_DEBUG(SU->getInstr()->dump());
      // }
      if (!SU) {
        CandPolicy NoPolicy;
        TopCand.reset(NoPolicy);
        pickNodeFromQueue(Top, NoPolicy, DAG->getTopRPTracker(), TopCand);
        assert(TopCand.Reason != NoCand && "failed to find a candidate");
        SU = TopCand.SU;
      }
      IsTopNode = true;
    } else if (RegionPolicy.OnlyBottomUp) {
      llvm_unreachable("Consider top down only");
    } else {
      llvm_unreachable("Consider top down only");
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
  assert(IsLoopBody && "Generic pickNodeFromQueue shouldn't be here");
  // GASS-specific heuristic
  if (tryPickNodeFromQueue(Zone, ZonePolicy, Cand))
    return;
  // fall-throught default heuristic
  llvm_unreachable("Should have returned");
}

//==------------------------------------------------------------------------==//
// GASS heuristic
//==------------------------------------------------------------------------==//
std::vector<int> GASSSchedStrategy::getSUScore(SUnit *SU) {
  std::vector<int> Score(SCHED_PRIORITY_SIZE, 0);
  // bias COPY (following the practice in GenericScheduler)
  computeCOPYScore(Score, SU);
  computeMathScore(Score, SU);
  computeLDSScore(Score, SU);
  computeLDGScore(Score, SU);  
  computeLDSIdxScore(Score, SU);
  computeFreeResourceScore(Score, SU);
  // bias instr that reads 
  // computeCarryInScore(Score, SU);
  // computeLatencyStallScore(Score, SU);
  // Fall through to order
  computeOrderScore(Score, SU);

  return Score;
}

// helpers
bool GASSSchedStrategy::isResourceFree(SUnit *SU) {
  const MCSchedClassDesc *SC = DAG->getSchedClass(SU);
  for (const MCWriteProcResEntry &PE :
       make_range(SchedModel->getWriteProcResBegin(SC),
                  SchedModel->getWriteProcResEnd(SC))) {
    unsigned ReservedUntil, InstanceIdx;
    std::tie(ReservedUntil, InstanceIdx) = 
        Top.getNextResourceCycle(PE.ProcResourceIdx, 0);
    if (ReservedUntil >= Top.getCurrCycle())
      // Resource still busy
      return false;
  }
  return true;
}

/// Returns true if this 
bool GASSSchedStrategy::isCritialResourceRequired(SUnit *SU) {
  const TargetSubtargetInfo &STI = DAG->MF.getSubtarget();
  unsigned SchedClass = SU->getInstr()->getDesc().getSchedClass();
  const MCSchedClassDesc *SCDesc = 
      STI.getSchedModel().getSchedClassDesc(SchedClass);

  for (const MCWriteProcResEntry &PRE :
        make_range(STI.getWriteProcResBegin(SCDesc),
                  STI.getWriteProcResEnd(SCDesc))) {
    assert(PRE.Cycles != 0);
    if (PRE.ProcResourceIdx == CritialFU)
      return true;
  }
  return false;
}

bool GASSSchedStrategy::isMathSU(SUnit *SU) {
  const MCSchedClassDesc *SC = DAG->getSchedClass(SU);
  if (StringRef(SC->Name).startswith("WriteHMMA") ||
      StringRef(SC->Name).startswith("WriteFP"))
    return true;
  else
    return false;
}

void GASSSchedStrategy::computeMathScore(std::vector<int> &Score, SUnit *SU) {
  // Requires math resource & resource is free
  if (isMathSU(SU) && isResourceFree(SU)) {
    assert(Ks.count(SU) != 0 && "math SU must has k value");
    if (Ks[SU] <= CurrentK)
      Score[SCHED_MATH] = 1;
  } else
    Score[SCHED_MATH] = 0;
}



void GASSSchedStrategy::computeCOPYScore(std::vector<int> &Score, SUnit *SU) {
  // TODO: fill this
  // Score[SCHED_COPY] = 0;
  return;
}


// Limit the number of live registers brought by LDSs
bool GASSSchedStrategy::isOkToIssueLDS(SUnit *SU) {
  if ((Ks[SU] <= CurrentK + 1) && isResourceFree(SU))
    return true;
  return false;
}

void GASSSchedStrategy::computeLDSScore(std::vector<int> &Score, SUnit *SU) {
  if (GASSInstrInfo::isLDS(*SU->getInstr())) {
    if (!isOkToIssueLDS(SU)) {
      if (Ks[SU] > CurrentK + 1)
        Score[SCHED_LDS] = -1; // Never issue LDS in previous k
      // Don't overwhelm LDST
      return;
    }
    Score[SCHED_LDS] = 1; 
  } else // do nothing
    return;
}

void GASSSchedStrategy::computeLDGScore(std::vector<int> &Score, SUnit *SU) {
  if (GASSInstrInfo::isLDG(*SU->getInstr())) {
    if (IssuedLDGs >= CurrentK*MaxLDGsPerK + MaxLDGsPerK-1) {
      Score[SCHED_LDG] = -1;
      return;
    }
    if (!isResourceFree(SU))
      // Don't overwhelm LDST
      return;
    Score[SCHED_LDG] = 1;
  } else // do nothing
    return;
}

void GASSSchedStrategy::computeLDSIdxScore(std::vector<int> &Score, SUnit *SU) {
  if (LdsDeps.contains(SU))
    Score[SCHED_LDS_IDX] = 1;
  else
    Score[SCHED_LDS_IDX] = 0;
}

void GASSSchedStrategy::computeFreeResourceScore(std::vector<int> &Score, 
                                                 SUnit *SU) {
  // assert(IsTopNode)?
  // Check is all required resources are free
  if (!isResourceFree(SU))
    return;
  // All required resources are free
  Score[SCHED_FREE_RESOURCE] = 1;

  // // Check if it is LDS
  // if (GASSInstrInfo::isLDS(*SU->getInstr()))
  //   if (!isOkToIssueLDS(SU))
  //     Score[SCHED_FREE_RESOURCE] = 0;
}

// void GASSSchedStrategy::computeCarryInScore(std::vector<int> &Score, 
//                                             SUnit* SU) {
//   // const MachineInstr *Instr = SU->getInstr();
//   // if (GASSInstrInfo::ifReadsCarryIn(*Instr))
//   //   Score[SCHED_CARRYIN] = 1;
// }

// // TODO: better to name "LatencyStall" as "WaitCycle"?
// void GASSSchedStrategy::computeLatencyStallScore(std::vector<int> &Score, 
//                                                  SUnit *SU) {
//   // int StallLatency = Top.getCurrCycle() - SU->TopReadyCycle;
//   // assert(StallLatency >= 0);
//   // // Force this number to be 0 for LDS
//   // if (GASSInstrInfo::isLDS(*SU->getInstr()))
//   //   StallLatency = 0;
//   // Score[SCHED_LATENCY] = StallLatency;
// }

void GASSSchedStrategy::computeOrderScore(std::vector<int> &Score, SUnit *SU) {
  Score[SCHED_ORDER] = -SU->NodeNum;
  // Give LDS a second chance
  if (GASSInstrInfo::isLDS(*SU->getInstr()) && (Ks[SU] <= CurrentK + 1))
    Score[SCHED_ORDER] += 999;
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
  
  // dbgs() << "\n\n\nCurrent best candidate is:\n";
  // Q.elements()[MaxIdx]->getInstr()->dump();
  // DAG->dumpNodeName(*Q.elements()[MaxIdx]);
  
  // dbgs() << ", score of which is:\n";
  // dbgs() << "{";
  // for (int i=0; i<SCHED_PRIORITY_SIZE; ++i)
  //   dbgs() << Scores[MaxIdx][i] << ", ";
  // dbgs() << "}\n";

  // dbgs() << "\nScore of other nodes:\n";
  // for (int i = 0; i < Scores.size(); ++i) {
  //   Q.elements()[i]->getInstr()->dump();
  //   DAG->dumpNodeName(*Q.elements()[i]);
  //   dbgs() << ", score of which is:\n";
  //   dbgs() << "{";
  //   for (int j=0; j<SCHED_PRIORITY_SIZE; ++j)
  //     dbgs() << Scores[i][j] << ", ";
  //   dbgs() << "}\n";
  // }

  // dbgs() << "CurrentK: " << CurrentK << "\n";

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
  assert(!IsLoopBody && "Specialized Scheduler won't be here");
  return GenericScheduler::tryCandidate(Cand, TryCand, Zone);
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

      // update CurrentK
      RemMaths[CurrentK].erase(SU);
      if (RemMaths[CurrentK].empty())
        CurrentK++;
      if (GASSInstrInfo::isLDG(*SU->getInstr()))
        IssuedLDGs++;
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
  // track outer loop index
  CurrentK = 0;
}

// Per-region initialize
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

  // Construct ks if required
  if (IsLoopBody)
    constructKs();
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

  // record critial resource
  for (auto iter : Resources) {
    if (iter.second > MaxCycles) {
      CritialFU = iter.first;
      MaxCycles = iter.second;
    }
  }
  StringRef CritialName = STI.getSchedModel().getProcResource(CritialFU)->Name;
  errs() << "Critial FU: " << CritialName
         << "\t Cycles: " << MaxCycles << "\n";
  // if (IsLoopBody)
  //   DAG->viewGraph();
  if (CritialName.equals("SM70UnitTensorCore") || CritialName.equals("SM70UnitFP32")) {
    IsLoopBody &= true;
    // DAG->viewGraph();
  } 
  // else
  //   IsLoopBody &= false;
}

static int collectCycles(const MCSchedClassDesc *SC, const TargetSchedModel *SchedModel,
                         SUnit *SU) {
  int Cycles = 0;
  for (const MCWriteProcResEntry &PE : 
       make_range(SchedModel->getWriteProcResBegin(SC), 
                  SchedModel->getWriteProcResEnd(SC))) {
    Cycles = std::max(Cycles, int(PE.Cycles));
  }
  return Cycles;
}

void GASSSchedStrategy::constructKs() {
  assert(IsLoopBody);
  RemMaths.clear();

  // DAG->MF.dump();

  DenseSet<SUnit*> Visited;
  std::vector<SUnit*> WorkList;
  std::vector<SUnit*> MathSUs;

  for (SUnit &SU : DAG->SUnits) {
    if (SU.NumPreds == 0)
      WorkList.push_back(&SU);
  }

  // 1. Get k for all math SUs
  while (!WorkList.empty()) {
    SUnit *Curr = WorkList.back();
    WorkList.pop_back();
    Visited.insert(Curr);

    for (const SDep &Succ : Curr->Succs) {
      if (!Visited.contains(Succ.getSUnit()) && Succ.getKind() == SDep::Data)
        WorkList.push_back(Succ.getSUnit());
    }

    assert(Curr);
    if (isMathSU(Curr)) {
      MathSUs.push_back(Curr);
      Ks[Curr] = 0;
      for (const SDep &Pred : Curr->Preds) {
        if (Pred.getKind() != SDep::Data)
          continue;
        if (Ks.count(Pred.getSUnit()) != 0) {
          Ks[Curr] = Ks[Pred.getSUnit()] + 1;
          break;
        }
      }
      RemMaths[Ks[Curr]].insert(Curr);
    }
  }

  // 2. Get k for all LDSs
  DenseSet<SUnit*> FirstLDSs;
  for (SUnit *MathSU : MathSUs) {
    assert(Ks.count(MathSU) != 0);
    int K = Ks[MathSU];

    for (const SDep &Pred : MathSU->Preds) {
      if (Pred.getKind() != SDep::Data)
        continue;
      SUnit *PredSU = Pred.getSUnit();
      const MCSchedClassDesc *SC = DAG->getSchedClass(PredSU);
      if (StringRef(SC->Name).startswith("WriteLDS")) {
        Ks[PredSU] = K;
        if (K == 1)
          FirstLDSs.insert(PredSU);
      }
    }
  }

  int MaxK = RemMaths.size();

  // Note: prefetched lds are considered of group (MaxK)
  for (SUnit &SU : DAG->SUnits) {
    if (!GASSInstrInfo::isLDS(*SU.getInstr()))
      continue;
    if (Ks.count(&SU) == 0)
      Ks[&SU] = MaxK;
  }

  // 3. Record index computation of the first group (k) of LDS
  //      to bias LDS index computation
  for (SUnit *Lds : FirstLDSs) {
    DenseSet<SUnit*> visited;
    std::vector<SUnit*> WorkList;

    for (const SDep &Pred : Lds->Preds)
      if (Pred.getKind() == SDep::Data)
        WorkList.push_back(Pred.getSUnit());

    while (!WorkList.empty()) {
      SUnit *Curr = WorkList.back();
      WorkList.pop_back();

      LdsDeps.insert(Curr);
      visited.insert(Curr);
      for (const SDep &Pred : Curr->Preds) {
        if (Pred.getKind() == SDep::Data && !visited.contains(Pred.getSUnit())) {
          WorkList.push_back(Pred.getSUnit());
        }
      }
    }
  }

  // 4. Plan ahead for LDGs
  int MathCycPerK = 0, LDSCycPerK = 0, ToalLDGCycles = 0, LDGCycles = 0, TotalLDGs = 0;
  for (SUnit *SU : RemMaths[0])
    MathCycPerK += collectCycles(DAG->getSchedClass(SU), SchedModel, SU);
  for (SUnit *SU : FirstLDSs)
    LDSCycPerK += collectCycles(DAG->getSchedClass(SU), SchedModel, SU);
  for (SUnit &SU : DAG->SUnits) 
    if (GASSInstrInfo::isLDG(*SU.getInstr())) {
      ToalLDGCycles += collectCycles(DAG->getSchedClass(&SU), SchedModel, &SU);
      LDGCycles = collectCycles(DAG->getSchedClass(&SU), SchedModel, &SU);
      TotalLDGs++;
    }

  MaxLDGsPerK = std::max((MathCycPerK - LDSCycPerK) / LDGCycles, 1);

  outs() << "Math Cycles Per K: " << MathCycPerK << "\n"
         << "LDS Cycles Per K: " << LDSCycPerK << "\n"
         << "LDG Cycles: " << ToalLDGCycles << "\n"
         << "Max LDGs Per K: " << MaxLDGsPerK << "\n";
  
  // // 5. dump
  // for (auto iter : Ks) {
  //   iter.getFirst()->getInstr()->dump();
  //   errs() << iter.second << "\n";
  // }
}