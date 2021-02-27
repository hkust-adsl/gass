#ifndef LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSASMSTREAMER_H
#define LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSASMSTREAMER_H

#include "GASSTargetStreamer.h"

namespace llvm {
class GASSTargetAsmStreamer : public GASSTargetStreamer {
  formatted_raw_ostream &OS;
public:
  GASSTargetAsmStreamer(MCStreamer &S, formatted_raw_ostream &OS);

  void emitDwarfFileDirective(StringRef Directive) override;

  // Override pure virtual functions defined in GASSTargetStreamer
  void emitAttributes(unsigned SmVersion) override;

  // Do nothing for now.
  void emitNvInfo(std::vector<std::unique_ptr<NvInfo>> &Info) override {};
  void emitNvInfoFunc(std::vector<std::unique_ptr<NvInfo>> &Info) override {};
};
}

#endif