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
  static bool isLoad(const MachineInstr &MI);
  static bool isStore(const MachineInstr &MI);

  // Encode wait barrier (3+3+6=12 bits)
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