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
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/TargetSchedule.h"

#include <map>

using namespace llvm;

#define DEBUG_TYPE "gass-stall-setting"

// This pass sets stall cycles for each instructions
char GASSStallSetting::ID = 0;

INITIALIZE_PASS(GASSStallSetting, "gass-stall-setting",
                "Setting Instruction Stall Cycles", true, true)


bool GASSStallSetting::runOnMachineFunction(MachineFunction &MF) {
  const auto &ST = MF.getSubtarget<GASSSubtarget>();
  const GASSInstrInfo *GII = ST.getInstrInfo();
  const GASSRegisterInfo *GRI = ST.getRegisterInfo();

  SchedModel.init(&ST);
  if (!SchedModel.hasInstrSchedModel())
    llvm_unreachable("GASS requires InstrSchedModel.");

  for (MachineBasicBlock &MBB : MF) {
    // Record activate regs
    std::map<Register*, int> ActivateRegs;
    for (auto iter = MBB.begin(); iter != MBB.end(); ++iter) {
      MachineInstr &MI = *iter;
      // minimum stall cycle: 2 (Maybe we can change this to 1?)
      int Stalls = 2; 

      // TODO: use enums to interpret TSFlags
      bool IsFixedLat = MI.getDesc().TSFlags & 1;
      int Lat = SchedModel.computeInstrLatency(&MI, false);

      // 1. Insert def regs as active regs
      if (IsFixedLat) {
        for (const MachineOperand &MOP : MI.defs()) {
          if (MOP.isReg()) {
            // We don't consider WAW here (why?)
            Register Reg = MOP.getReg();
            // Update
            if (ActivateRegs.find(&Reg) == ActivateRegs.end()) 
              ActivateRegs[&Reg] = Lat;
            else
              ActivateRegs[&Reg] = std::max(ActivateRegs[&Reg], Lat);
          }
        }
      } else {
        // We stall instructions with var latency to stall for at least 2 cycles
        Stalls = 2;
      }

      // 2. Peek the next instr
      auto next_iter = std::next(iter);
      if (next_iter != MBB.end()) {
        // if the next instr reads any of the current active
        for (const auto &x : ActivateRegs) {
          Register *ARegs = x.first;
          int ALat = x.second;

          for(const MachineOperand &MOP : next_iter->uses()) {
            if (MOP.isReg()) 
              if (GRI->regsOverlap(MOP.getReg(), *ARegs))
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

      // Special rule for BRA
      if (MI.isBranch())
        Stalls = std::max(Stalls, int(7));

      // (Optional) Debug
      LLVM_DEBUG(MI.dump());
      LLVM_DEBUG(dbgs() << "[Stalls: " << Stalls << "]\n");

      // 4. Record Stall info
      assert(Stalls >= 1);
      StallCycleMap[&MI] = Stalls;
    } // for each MI
  }

  return false;
}