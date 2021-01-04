#ifndef LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSELFSTREAMER_H
#define LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSELFSTREAMER_H

#include "GASSTargetStreamer.h"
#include "llvm/MC/MCELFStreamer.h"

namespace llvm {
class GASSTargetELFStreamer : public GASSTargetStreamer {
public:
  MCELFStreamer &getStreamer();
  GASSTargetELFStreamer(MCStreamer &S, const MCSubtargetInfo &STI);
};
} // namespace llvm

#endif