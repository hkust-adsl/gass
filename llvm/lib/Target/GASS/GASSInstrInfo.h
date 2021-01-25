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

  bool expandPostRAPseudo(MachineInstr &MI) const override;

  static bool isLoad(const MachineInstr &MI);
  static bool isStore(const MachineInstr &MI);

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