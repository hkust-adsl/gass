#ifndef LLVM_LIB_TARGET_GASS_GASSSUBTARGET_H
#define LLVM_LIB_TARGET_GASS_GASSSUBTARGET_H

#include "GASSInstrInfo.h"
#include "GASSISelLowering.h"
#include "GASSFrameLowering.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"

#define GET_SUBTARGETINFO_HEADER
#include "GASSGenSubtargetInfo.inc"

namespace llvm {
class GASSSubtarget : public GASSGenSubtargetInfo {
  GASSRegisterInfo RegInfo;
  GASSInstrInfo InstrInfo;
  GASSTargetLowering TLInfo;
  GASSFrameLowering FrameLowering;

  unsigned SmVersion = 70;

public:
  GASSSubtarget(const Triple &TT, StringRef CPU, StringRef FS, 
                const TargetMachine &TM);

  // Parses features string setting specified subtarget options. The
  // definition of this function is auto-generated by tblgen.
  void ParseSubtargetFeatures(StringRef CPU, StringRef TuneCPU, StringRef FS);
  
  // Override functions to get subtarget info
  const GASSRegisterInfo *getRegisterInfo() const override {
    return &RegInfo;
  }
  const GASSInstrInfo *getInstrInfo() const override {
    return &InstrInfo;
  }
  const GASSTargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }
  const GASSFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }

  bool enableMachineScheduler() const override;
  bool enablePostRAScheduler() const override;

  void overrideSchedPolicy(MachineSchedPolicy &Policy, 
                           unsigned NumRegionInstrs) const override;

  bool enableEarlyIfConversion() const override;

  unsigned getConstantOffset(unsigned IntNo) const;

protected:
  //
public:  
  // NVGPU specific info
  unsigned getSmVersion() const { return SmVersion; }
  unsigned getParamBase() const;
};
} // namespace llvm

#endif