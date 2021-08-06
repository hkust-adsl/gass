#include "GASS.h"
#include "GASSInstrInfo.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define DEBUG_TYPE "gass-machine-dce"
#define CPASS_NAME "GASS LDG Sink"

namespace {
class GASSMachineDCE : public MachineFunctionPass {
  const GASSInstrInfo *TII = nullptr;
  const MachineRegisterInfo *MRI = nullptr;
public:
  static char ID;

  GASSMachineDCE() : MachineFunctionPass(ID) {
    initializeGASSMachineDCEPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return CPASS_NAME;
  }

private:
  bool isDead(MachineInstr *MI);
  bool eliminateDeadMI(MachineFunction &MF);
};
} // anonymous namespace

char GASSMachineDCE::ID = 0;

INITIALIZE_PASS(GASSMachineDCE, DEBUG_TYPE, CPASS_NAME, false, false)

bool GASSMachineDCE::isDead(MachineInstr *MI) {
  if (MI->isInlineAsm())
    return false;

  // Don't delete instrs with side effects.
  bool SawStore = false;
  if (!MI->isSafeToMove(nullptr, SawStore) && !MI->isPHI())
    return false;

  // Examine each operand.
  for (unsigned i = 0, e = MI->getNumOperands(); i != e; ++i) {
    const MachineOperand &MO = MI->getOperand(i);
    if (!MO.isReg() || !MO.isDef())
      continue;

    Register Reg = MO.getReg();
    if (Reg.isPhysical())
      continue;
    
    if (!MRI->use_empty(Reg))
      return false;
  }

  // If there are no defs with uses, the instruction is dead;
  return true;
}

bool GASSMachineDCE::eliminateDeadMI(MachineFunction &MF) {
  bool AnyChange = false;
  // Loop over all instructions in all blocks, from bottom to top, so that it's more 
  // likely that chains for dependent but ultimately dead instructions will be cleaned up.
  for (MachineBasicBlock *MBB : post_order(&MF)) {
    // Now scan the instructions and delete dead ones.
    for (MachineBasicBlock::reverse_iterator MII = MBB->rbegin(),
                                             MIE = MBB->rend();
         MII != MIE;) {
      MachineInstr *MI = &*MII++;

      if (isDead(MI)) {
        LLVM_DEBUG(dbgs() << "Remove " << *MI);
        MI->eraseFromParent();
      }
    }
  }

  return AnyChange;
}

bool GASSMachineDCE::runOnMachineFunction(MachineFunction &MF) {
  bool MadeChange = false;

  MRI = &MF.getRegInfo();

  MadeChange = eliminateDeadMI(MF);
  while (MadeChange && eliminateDeadMI(MF));

  return MadeChange;
}

//------------------------- Public Interface -----------------------------------
FunctionPass *llvm::createGASSMachineDCEPass() {
  return new GASSMachineDCE();
}