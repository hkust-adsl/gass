#ifndef LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSELFSTREAMER_H
#define LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSELFSTREAMER_H

#include "GASSTargetStreamer.h"
#include "llvm/MC/MCELFStreamer.h"

namespace llvm {
class GASSTargetELFStreamer : public GASSTargetStreamer {
  const MCSubtargetInfo &STI;
  unsigned SmVersion = 70; // Can we read this from Subtarget?
public:
  MCELFStreamer &getStreamer();
  GASSTargetELFStreamer(MCStreamer &S, const MCSubtargetInfo &STI);

  // override virtual functions declared in GASSTargetStreamer
  void emitAttributes(unsigned SmVersion) override;
};
} // namespace llvm

#endif