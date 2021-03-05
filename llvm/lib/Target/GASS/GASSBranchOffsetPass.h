#ifndef LLVM_LIB_TARGET_GASS_GASSBRANCHOFFSETPASS_H
#define LLVM_LIB_TARGET_GASS_GASSBRANCHOFFSETPASS_H

#include "GASS.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/PassRegistry.h"

namespace llvm {
class GASSBranchOffset : public MachineFunctionPass {
  /// Record MI offset in byte
  DenseMap<const MachineInstr*, uint64_t> MIOffsets;
  /// Record MBB offset in byte
  DenseMap<const MachineBasicBlock*, uint64_t> MBBOffsets;

  /// BasicBlockInfo - Information about the offset and size of a single
  /// basic block.
  struct BasicBlockInfo {
    /// Offset - Distance from the beginning of the function to the beginning
    /// of this basic block.
    ///
    /// The offset is always aligned as required by the basic block.
    unsigned Offset = 0;

    /// Size - Size of the basic block in bytes.  If the block contains
    /// inline assembly, this is a worst case estimate.
    ///
    /// The size does not include any alignment padding whether from the
    /// beginning of the block, or from an aligned jump table at the end.
    unsigned Size = 0;

    BasicBlockInfo() = default;

    /// Compute the offset immediately following this block.
    unsigned postOffset() const { return Offset + Size; }
  };

  SmallVector<BasicBlockInfo, 16> BlockInfo;

  MachineFunction *MF = nullptr;
  const TargetInstrInfo *TII = nullptr;

public:
  static char ID;

  GASSBranchOffset() : MachineFunctionPass(ID) {
    initializeGASSBranchOffsetPass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  /// Query result
  const DenseMap<const MachineInstr*, uint64_t> &getMIOffsets() const {
    return MIOffsets;
  }

  const DenseMap<const MachineBasicBlock*, uint64_t> &getMBBOffsets() const {
    return MBBOffsets;
  }

  StringRef getPassName() const override {
    return "Collecting MI & MBB offset in byte";
  }

private:
  void scanFunction();

  // void getAnalysisUsage(AnalysisUsage &AU) const override {
  //   AU.setPreservesAll();
  // }
};
} // namespace llvm

#endif