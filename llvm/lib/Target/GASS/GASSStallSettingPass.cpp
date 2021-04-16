//==--------------------------------------------------------------------==//
//
// Setting stall cycles to prevent RAW hazards for instructions with fixed
// latency.
//
// Some factors have not been taken into consideration yet (FIXME)
// 1. Stall due to tput limit
// 2. Considering WAW (predicated executions are not considered as branch)
// 3. Across basic block boundary. (For better performance)
//
//==--------------------------------------------------------------------==//

#include "GASS.h"
#include "GASSStallSettingPass.h"
#include "MCTargetDesc/GASSMCTargetDesc.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/TargetSchedule.h"
#include "llvm/MC/MCSchedule.h"
#include "llvm/Support/ErrorHandling.h"

#include <map>

using namespace llvm;

#define DEBUG_TYPE "gass-stall-setting"

// This pass sets stall cycles for each instructions
char GASSStallSetting::ID = 0;

INITIALIZE_PASS(GASSStallSetting, "gass-stall-setting",
                "Setting Instruction Stall Cycles", true, true)

namespace {
class ResourceReserveTable {
  /// PIdx => Reserved until
  DenseMap<unsigned, int> Table;

  /// Cache SchedModel
  const TargetSchedModel &SchedModel;
public:
  ResourceReserveTable(const TargetSchedModel &SchedModel)
    : SchedModel(SchedModel) {
    for (unsigned PIdx = 1; 
         PIdx != SchedModel.getNumProcResourceKinds(); ++PIdx) {
      Table[PIdx] = 0;
    }
  }

  /// @param Stalls Update if stalled by resource
  void visitInstr(const MachineBasicBlock::iterator &CurrIter, 
                  int CurrCycle, int &Stalls, const MachineBasicBlock &MBB);
}; // class ResourceReserveTable
} // anonymous namespace

//--------- ResourceReserveTable -------------------
void ResourceReserveTable::visitInstr(
    const MachineBasicBlock::iterator &CurrIter, int CurrCycle, 
    int &Stalls, const MachineBasicBlock &MBB) {
  // Stall cycle caused by resource require ment
  int MaxResStall = 0;
  // bool IsLDST = false;

  // Peek the next instr
  const auto NextIter = std::next(CurrIter);

  // Last instr, do nothing
  if (NextIter == MBB.end())
    return;

  // 1. Update Resource Reserve Table
  const MCSchedClassDesc *SC = SchedModel.resolveSchedClass(&(*CurrIter));
  for (TargetSchedModel::ProcResIter
    PI = SchedModel.getWriteProcResBegin(SC),
    PE = SchedModel.getWriteProcResEnd(SC); PI != PE; ++PI) {
    unsigned PIdx = PI->ProcResourceIdx;
    unsigned PCycle = PI->Cycles;

    // // TODO: should query subtarget
    // if (StringRef(SchedModel.getResourceName(PIdx)).contains("LDST"))
    //   IsLDST = true;

    // Update Table
    // if (Table[PIdx] > CurrCycle) {
    //   // TODO: Or we can "forget" previous ...?
    //   Table[PIdx] += PCycle;
    // } else 
      Table[PIdx] = CurrCycle + PCycle;
  }

  // 2. Update MaxResStall
  const MCSchedClassDesc *NextSC = SchedModel.resolveSchedClass(&*NextIter);
  for (TargetSchedModel::ProcResIter
    PI = SchedModel.getWriteProcResBegin(NextSC),
    PE = SchedModel.getWriteProcResEnd(NextSC); PI != PE; ++PI) {
    unsigned PIdx = PI->ProcResourceIdx;

    // Update MaxResStall
    if (Table[PIdx] > CurrCycle)
      MaxResStall = std::max(MaxResStall, Table[PIdx] - CurrCycle);
  }

  // Update Stalls
  // Cap this to 2 cycles
  MaxResStall = std::min(2, MaxResStall);
  Stalls = std::max(Stalls, MaxResStall);
  // TODO: We have special rules for LDST
}


bool GASSStallSetting::runOnMachineFunction(MachineFunction &MF) {
  const auto &ST = MF.getSubtarget<GASSSubtarget>();
  const GASSInstrInfo *GII = ST.getInstrInfo();
  const GASSRegisterInfo *GRI = ST.getRegisterInfo();

  SchedModel.init(&ST);
  if (!SchedModel.hasInstrSchedModel())
    llvm_unreachable("GASS requires InstrSchedModel.");

  for (MachineBasicBlock &MBB : MF) {
    // Record activate regs
    std::map<Register, int> ActivateRegs;
    ResourceReserveTable ResRT(SchedModel);
    /// Cycle before current instr is issued
    int CurrCycle = 0;
    for (auto iter = MBB.begin(); iter != MBB.end(); ++iter) {
      MachineInstr &MI = *iter;
      // minimum stall cycle: 1 // (Maybe we can change this to 1?)
      int Stalls = 1; 

      // TODO: use enums to interpret TSFlags
      bool IsFixedLat = MI.getDesc().TSFlags & 1;
      int Lat = SchedModel.computeInstrLatency(&MI, false);

      // 1. Insert def regs as active regs
      if (IsFixedLat) {
        for (const MachineOperand &MOP : MI.defs()) {
          if (MOP.isReg() && !GRI->isConstantPhysReg(MOP.getReg())) {
            // We don't consider WAW here (why?)
            Register Reg = MOP.getReg();
            // Update
            if (ActivateRegs.find(Reg) == ActivateRegs.end()) 
              ActivateRegs[Reg] = Lat;
            else
              ActivateRegs[Reg] = std::max(ActivateRegs[Reg], Lat);
          }
        }
      } else {
        // For instr with variable latency, it must stall for at lease 2 cycles
        // if the next instr waits on the barrier it just sets

        // Get barriers that current instr sets
        int CurrWARBar = GII->decodeReadBarrier(MI);
        int CurrRAWBar = GII->decodeWriteBarrier(MI);
        // Peek the next instr
        auto next_iter = std::next(iter);
        if (next_iter != MBB.end()) {
          DenseSet<int> NextBarMask = GII->decodeBarrierMask(*next_iter);
          if (NextBarMask.contains(CurrWARBar) || 
              NextBarMask.contains(CurrRAWBar))
            // force it to stall for 2 cycles
            Stalls = 2;
        }
      }

      // 2. Peek the next instr
      auto next_iter = std::next(iter);
      if (next_iter != MBB.end()) {
        // if the next instr reads any of the current active
        for (const auto &x : ActivateRegs) {
          Register ARegs = x.first;
          int ALat = x.second;

          for(const MachineOperand &MOP : next_iter->uses()) {
            if (MOP.isReg()) 
              if (GRI->regsOverlap(MOP.getReg(), ARegs))
                Stalls = std::max(ALat, Stalls);
          }
        }
      } else { // This is the last instr
        // The last instr needs to wait on all activate regs
        for (const auto &x : ActivateRegs) {
          int ALat = x.second;
          Stalls = std::max(ALat, Stalls);
        }
      }

      // 3. Update activate regs
      for (auto areg_iter = ActivateRegs.begin(); 
                areg_iter != ActivateRegs.end(); ) {
        int ALat = areg_iter->second;
        ALat -= Stalls;

        if (ALat <= 0) 
          areg_iter = ActivateRegs.erase(areg_iter);
        else { // Update latency
          areg_iter->second = ALat;
          ++areg_iter;
        }
      }

      // 4. May stall because the pipe is busy
      ResRT.visitInstr(iter, CurrCycle, Stalls, MBB);

      // Special rules
      if (MI.isBranch())
        Stalls = std::max(Stalls, 7);
      if (MI.getOpcode() == GASS::BAR || MI.getOpcode() == GASS::BAR_DEFER)
        Stalls = std::max(Stalls, 5);

      // (Optional) Debug
      LLVM_DEBUG(MI.dump());
      LLVM_DEBUG(dbgs() << "[Stalls: " << Stalls << "]\n");

      // 4. Record Stall info
      assert(Stalls >= 1);
      StallCycleMap[&MI] = Stalls;
      CurrCycle += Stalls;
    } // for each MI
  } // for each MBB

  return false;
}