#include "GASSBranchOffsetPass.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

using namespace llvm;

#define DEBUG_TYPE "gass-branch-offset"

char GASSBranchOffset::ID = 0;

INITIALIZE_PASS(GASSBranchOffset, "gass-branch-offset", 
                "Collecting MI & MBB offset in byte", true, true)

void GASSBranchOffset::scanFunction() {
  BlockInfo.clear();
  BlockInfo.resize(MF->getNumBlockIDs());

  // First, compute the size of all basic blocks.
  for (MachineBasicBlock &MBB : *MF) {
    uint64_t Size = 0;
    for (const MachineInstr &MI : MBB)
      Size += TII->getInstSizeInBytes(MI);
    BlockInfo[MBB.getNumber()].Size = Size;
  }

  // Second, update MBB offset
  unsigned PrevNum = MF->begin()->getNumber();
  for (auto &MBB :
       make_range(std::next(MachineFunction::iterator(MF->begin())), 
                                                      MF->end())) {
    unsigned Num = MBB.getNumber();
    // Get the offset and known bits at the end of the layout predecessor.
    // Include the alignment of the current block.
    BlockInfo[Num].Offset = BlockInfo[PrevNum].postOffset();

    MBBOffsets[&MBB] = BlockInfo[Num].Offset;

    PrevNum = Num;
  }

  // Update Instr offset
  for (MachineBasicBlock &MBB : *MF) 
    for (const MachineInstr &MI : MBB) {
      uint64_t Offset = MBBOffsets.lookup(&MBB);
      MIOffsets[&MI] = Offset;
      Offset += TII->getInstSizeInBytes(MI);
    }
}

bool GASSBranchOffset::runOnMachineFunction(MachineFunction &mf) {
  MF = &mf;

  const TargetSubtargetInfo &ST = MF->getSubtarget();
  TII = ST.getInstrInfo();

  // Renumber all of the machine basic blocks in the function, guaranteeing that
  // the numbers agree with the position of the block in the function.
  MF->RenumberBlocks();

  scanFunction();

  return false;
}