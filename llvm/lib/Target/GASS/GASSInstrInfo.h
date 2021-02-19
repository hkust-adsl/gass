#ifndef LLVM_LIB_TARGET_GASS_GASSINSTRINFO_H
#define LLVM_LIB_TARGET_GASS_GASSINSTRINFO_H

#include "GASSRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

#define GET_INSTRINFO_HEADER
#include "GASSGenInstrInfo.inc"

namespace llvm {
class GASSSubtarget;

class GASSInstrInfo : public GASSGenInstrInfo {
public:
  /// Emit instructions to copy a pair of physical registers.
  ///
  /// This function should support copies within any legal register class as
  /// well as any cross-class copies created during instruction selection.
  ///
  /// The source and destination registers may overlap, which may require a
  /// careful implementation when multiple copy instructions are required for
  /// large registers. See for example the ARM target.
  void copyPhysReg(MachineBasicBlock &MBB,
                   MachineBasicBlock::iterator MI, const DebugLoc &DL,
                   MCRegister DestReg, MCRegister SrcReg,
                   bool KillSrc) const override;

  // Branch analyzsis.
  bool analyzeBranch(MachineBasicBlock &MBB, MachineBasicBlock *&TBB,
                     MachineBasicBlock *&FBB,
                     SmallVectorImpl<MachineOperand> &Cond,
                     bool AllowModify) const override;
  unsigned removeBranch(MachineBasicBlock &MBB,
                        int *BytesRemoved = nullptr) const override;
  unsigned insertBranch(MachineBasicBlock &MBB, MachineBasicBlock *TBB,
                        MachineBasicBlock *FBB, ArrayRef<MachineOperand> Cond,
                        const DebugLoc &DL,
                        int *BytesAdded = nullptr) const override;
  bool
  reverseBranchCondition(SmallVectorImpl<MachineOperand> &Cond) const override;

  /// Returns true on success.
  bool PredicateInstruction(MachineInstr &MI, 
                            ArrayRef<MachineOperand> Pred) const override;
  
  // required by ifconverter
  // TODO: update heuristic.
  bool isProfitableToDupForIfCvt(MachineBasicBlock &MBB,
                                 unsigned NumInstrs,
                                 BranchProbability Probability) const override {
    return true;
  }

  bool isProfitableToIfCvt(MachineBasicBlock &MBB, unsigned NumCycles,
                           unsigned ExtraPredCycles, 
                           BranchProbability Probability) const override {
    return true;
  }

  bool expandPostRAPseudo(MachineInstr &MI) const override;

  // Query instr type
  static bool isLoad(const MachineInstr &MI);
  static bool isStore(const MachineInstr &MI);
  static bool isLDG(const MachineInstr &MI);
  static bool isLDS(const MachineInstr &MI);
  static bool isLDC(const MachineInstr &MI);
  static bool isSTG(const MachineInstr &MI);
  static bool isSTS(const MachineInstr &MI);

  // return register of memory operand register (MemOperandReg)
  // return nullptr if this MI has no MemOperandReg
  static MachineOperand* getMemOperandReg(MachineInstr &MI);

  // Encode wait barrier (3+3+6=12 bits)
  static void initializeFlagsEncoding(MachineInstr &MI);
  static void encodeReadBarrier(MachineInstr &MI, unsigned BarIdx);
  static void encodeWriteBarrier(MachineInstr &MI, unsigned BarIdx);
  static void encodeBarrierMask(MachineInstr &MI, unsigned BarIdx);

  // Encode stall cycles (4 bits)
  static void encodeStallCycles(MachineInstr &MI, unsigned Stalls);
};

namespace GASS {
enum TsflagMask : unsigned {
  FixedLatMask = 0x1,
};
} // namespace GASS

} // namespace llvm

#endif
