#include "GASS.h"
#include "MCTargetDesc/GASSMCTargetDesc.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/SlotIndexes.h"
#include "llvm/PassRegistry.h"

#include <map>

using namespace llvm;

#define PASS_NAME "Compute register pressure at each line"
#define DEBUG_TYPE "reg-pressure-comp"

namespace {
class RegPressureCompute : public MachineFunctionPass {
  const MachineRegisterInfo *MRI = nullptr;
  LiveIntervals *LIS = nullptr;

  std::map<const MachineInstr*, unsigned> NumActiveVReg1;
  std::map<const MachineInstr*, unsigned> NumActiveVReg32;
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
}; // class RegPressureCompute
} // anonymous namespace

char RegPressureCompute::ID = 0;

INITIALIZE_PASS(RegPressureCompute, DEBUG_TYPE, PASS_NAME, true, true)

void RegPressureCompute::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<LiveIntervals>();
  AU.setPreservesAll();
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
    for (const MachineInstr &MI : MBB) {
      outs() << "#VReg1: "  << NumActiveVReg1[&MI] << " "
             << "#VReg32: " << NumActiveVReg32[&MI] << "   ";
      MI.print(outs());
      outs().flush();
    }
  }
}

// Ref: GCNRegPressure.cpp::printLivesAt
bool RegPressureCompute::runOnMachineFunction(MachineFunction &MF) {
  MRI = &MF.getRegInfo();
  const TargetRegisterInfo *TRI = MF.getSubtarget().getRegisterInfo();
  LIS = &getAnalysis<LiveIntervals>();

  for (const MachineBasicBlock &MBB : MF) {
    for (const MachineInstr &MI : MBB) {
      NumActiveVReg1[&MI] = 0;
      NumActiveVReg32[&MI] = 0;
      SlotIndex SI = LIS->getInstructionIndex(MI);

      // Visit every LiveInterval (vreg) to see if ...
      for (unsigned i = 0; i != MRI->getNumVirtRegs(); ++i) {
        Register Reg = Register::index2VirtReg(i);
        const TargetRegisterClass *RC = MRI->getRegClass(Reg);
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