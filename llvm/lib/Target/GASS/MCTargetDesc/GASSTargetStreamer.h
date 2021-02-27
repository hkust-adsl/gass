#ifndef LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSTARGETSTREAMER_H
#define LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSTARGETSTREAMER_H

#include "llvm/MC/MCStreamer.h"

#include <vector>
#include <memory>

namespace llvm {
class Module;
class MachineFunction;
class NvInfo;

class GASSTargetStreamer : public MCTargetStreamer {
protected:
  /// Required by predicateSymtabIndex in GASSTargetELFStreamer
  std::vector<MachineFunction *> MFs;
  unsigned NumMFs;
public:
  GASSTargetStreamer(MCStreamer &S);

  // for ELF header (arch, cuda version)
  virtual void emitAttributes(unsigned SmVersion) = 0;

  /// emit .nv.info (per module)
  virtual void emitNvInfo(std::vector<std::unique_ptr<NvInfo>> &Info) = 0;

  /// emit .nv.info.{name} (per function)
  virtual void emitNvInfoFunc(std::vector<std::unique_ptr<NvInfo>> &Info) = 0;

  /// cache MF index
  void visitMachineFunction(MachineFunction *MF) {
    MFs.push_back(MF);
  }

  /// cache NumMFs
  void recordNumMFs(unsigned NumMFs) { this->NumMFs = NumMFs; }
};
} // namespace llvm

#endif