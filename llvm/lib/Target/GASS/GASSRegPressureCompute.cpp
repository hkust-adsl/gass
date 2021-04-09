#include "GASS.h"
#include "MCTargetDesc/GASSMCTargetDesc.h"
#include "llvm/CodeGen/LiveInterval.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/SlotIndexes.h"
#include "llvm/InitializePasses.h"
#include "llvm/PassRegistry.h"
#include "llvm/Support/raw_ostream.h"

#include <map>

using namespace llvm;

#define PASS_NAME "Compute register pressure at each line"
#define DEBUG_TYPE "reg-pressure-comp"

namespace {
class RegPressureCompute : public MachineFunctionPass {
  const MachineRegisterInfo *MRI = nullptr;
  const TargetRegisterInfo *TRI = nullptr;
  LiveIntervals *LIS = nullptr;

  std::map<const MachineInstr*, unsigned> NumActiveVReg1;
  std::map<const MachineInstr*, unsigned> NumActiveVReg32;

  // collect livein & liveout (vreg)
  std::map<const MachineBasicBlock*, std::vector<Register>> LiveIns;
  std::map<const MachineBasicBlock*, std::vector<Register>> LiveOuts;
public:
  static char ID;
  
  RegPressureCompute() : MachineFunctionPass(ID) {
    initializeRegPressureComputePass(*PassRegistry::getPassRegistry());
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override;

  bool runOnMachineFunction(MachineFunction &MF) override;

private:
  void updateActiveNumRegs(Register Reg, const MachineInstr &MI);

  /// dump Number of active registers per instruction
  void dumpResult(MachineFunction &MF);

  /// compute virtual live-in and outs
  void computeLiveInsAndOuts(MachineFunction &MF);
}; // class RegPressureCompute
} // anonymous namespace

char RegPressureCompute::ID = 0;

INITIALIZE_PASS_BEGIN(RegPressureCompute, DEBUG_TYPE, PASS_NAME, false, false)
INITIALIZE_PASS_DEPENDENCY(SlotIndexes)
INITIALIZE_PASS_DEPENDENCY(LiveIntervals)
INITIALIZE_PASS_END(RegPressureCompute, DEBUG_TYPE, PASS_NAME, false, false)

void RegPressureCompute::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<LiveIntervals>();
  AU.addPreserved<LiveIntervals>();
  AU.addPreserved<SlotIndexes>();
  MachineFunctionPass::getAnalysisUsage(AU);
}

void RegPressureCompute::updateActiveNumRegs(Register Reg, 
                                             const MachineInstr &MI) {
  // Add register pressure
  PSetIterator PSetI = MRI->getPressureSets(Reg);
  for (; PSetI.isValid(); ++PSetI) {
    if (*PSetI == GASS::RegisterPressureSets::VReg1)
      NumActiveVReg1[&MI] += 1;
    else if (*PSetI == GASS::RegisterPressureSets::VReg32)
      // VReg32's weight is 2, VReg64's weight is 4, etc..
      NumActiveVReg32[&MI] += PSetI.getWeight() / 2;
    else 
      llvm_unreachable("Invalid Register Pressure Set.");
  }
}

void RegPressureCompute::dumpResult(MachineFunction &MF) {
  for (const MachineBasicBlock &MBB : MF) {
    // copy from MachineBasicBlock::print
    raw_ostream &OS = outs();

    OS << "\n";
    OS << MBB.getName() << ":\n";
    bool HasLineAttributes = false;

    // Print the preds of this block according to the CFG.
    if (!MBB.pred_empty()) {
      // Don't indent(2), align with previous line attributes.
      OS << "; predecessors: ";
      for (auto I = MBB.pred_begin(), E = MBB.pred_end(); I != E; ++I) {
        if (I != MBB.pred_begin())
          OS << ", ";
        OS << (*I)->getName();
      }
      OS << '\n';
      HasLineAttributes = true;
    }

    if (!MBB.succ_empty()) {
      // Print the successors
      OS.indent(2) << "successors: ";
      for (auto I = MBB.succ_begin(), E = MBB.succ_end(); I != E; ++I) {
        if (I != MBB.succ_begin())
          OS << ", ";
        OS << (*I)->getName();
      }
      OS << '\n';
      HasLineAttributes = true;
    }

    if (!MBB.livein_empty() && MRI->tracksLiveness()) {
      OS.indent(2) << "liveins: ";

      bool First = true;
      for (const auto &LI : MBB.liveins()) {
        if (!First)
          OS << ", ";
        First = false;
        OS << printReg(LI.PhysReg, TRI);
        if (!LI.LaneMask.all())
          OS << ":0x" << PrintLaneMask(LI.LaneMask);
      }
      HasLineAttributes = true;
    }

    // dump virtual live-ins
    if (LiveIns.find(&MBB) != LiveIns.end()) {
      OS.indent(2) << "liveins: ";

      int idx = 0;
      for (Register Reg : LiveIns[&MBB]) {
        const TargetRegisterClass *RC = MRI->getRegClass(Reg);
        OS << "%" << (Reg.id() - (1<<31)) << "(" << TRI->getRegClassName(RC)
                  << ")" << ", ";
        if (idx % 10 == 9) {
          OS << "\n";
          OS.indent(11);
        }
        idx++;
      }

      OS << "\n\n";        
    }


    for (const MachineInstr &MI : MBB) {
      outs() << "#VReg1: "  << NumActiveVReg1[&MI] << " "
             << "#VReg32: " << NumActiveVReg32[&MI] << "   ";
      MI.print(outs());
      outs().flush();
    }

    // dump virtual live-outs
    if (LiveOuts.find(&MBB) != LiveOuts.end()) {
      OS.indent(2) << "liveouts: ";

      int idx = 0;
      for (Register Reg : LiveOuts[&MBB]) {
        const TargetRegisterClass *RC = MRI->getRegClass(Reg);
        OS << "%" << (Reg.id() - (1<<31)) << "(" << TRI->getRegClassName(RC)
                  << ")" << ", ";
        if (idx % 10 == 9) {
          OS << "\n";
          OS.indent(12);
        }
        idx++;
      }
      OS << "\n";       
    }
  }
}

void RegPressureCompute::computeLiveInsAndOuts(MachineFunction &MF) {
  for (const MachineBasicBlock &MBB : MF) {
    for (unsigned i = 0; i != MRI->getNumVirtRegs(); ++i) {
      Register Reg = Register::index2VirtReg(i);
      if (LIS->hasInterval(Reg)) {
        const LiveInterval &LI = LIS->getInterval(Reg);
        if (LIS->isLiveInToMBB(LI, &MBB))
          LiveIns[&MBB].push_back(Reg);
        else if (LIS->isLiveOutOfMBB(LI, &MBB))
          LiveOuts[&MBB].push_back(Reg);
      }
    }
  }
}

// Ref: GCNRegPressure.cpp::printLivesAt
bool RegPressureCompute::runOnMachineFunction(MachineFunction &MF) {
  MRI = &MF.getRegInfo();
  TRI = MF.getSubtarget().getRegisterInfo();
  LIS = &getAnalysis<LiveIntervals>();

  // // Update liveness
  // for (unsigned i = 0; i != MRI->getNumVirtRegs(); ++i) {
  //   Register Reg = Register::index2VirtReg(i);
  //   assert(Reg.isVirtual());
  //   if (LIS->hasInterval(Reg)) {
  //     LIS->removeInterval(Reg);
  //     LIS->createAndComputeVirtRegInterval(Reg);
  //   }
  //   LIS->getInterval(Reg).verify();
  // }

  // collect livein & liveouts
  computeLiveInsAndOuts(MF);

  // Recored active regs
  for (const MachineBasicBlock &MBB : MF) {
    for (const MachineInstr &MI : MBB) {
      NumActiveVReg1[&MI] = 0;
      NumActiveVReg32[&MI] = 0;
      SlotIndex SI = LIS->getInstructionIndex(MI);

      // Visit every LiveInterval (vreg) to see if ...
      for (unsigned i = 0; i != MRI->getNumVirtRegs(); ++i) {
        Register Reg = Register::index2VirtReg(i);
        const TargetRegisterClass *RC = MRI->getRegClass(Reg);
        LIS->getInterval(Reg).verify();
        if (LIS->hasInterval(Reg)) {
          const LiveInterval &LI = LIS->getInterval(Reg);
          if (LI.hasSubRanges()) {
            for (const LiveInterval::SubRange &S : LI.subranges()) {
              if (!S.liveAt(SI)) 
                continue;
              updateActiveNumRegs(Reg, MI);
            }
          } else if (LI.liveAt(SI)) {
            updateActiveNumRegs(Reg, MI);
          }
        }
      }
    }
  }

  dumpResult(MF);

  NumActiveVReg1.clear();
  NumActiveVReg32.clear();

  return false;
}


//==------------------------------------------------------------------------==//
// public interface
//==------------------------------------------------------------------------==//
FunctionPass *llvm::createRegPressureComputePass() {
  return new RegPressureCompute();
}