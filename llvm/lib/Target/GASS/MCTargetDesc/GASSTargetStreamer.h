#ifndef LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSTARGETSTREAMER_H
#define LLVM_LIB_TARGET_GASS_MCTARGETDESC_GASSTARGETSTREAMER_H

#include "llvm/MC/MCStreamer.h"

namespace llvm {
class GASSTargetStreamer : public MCTargetStreamer {
public:
  GASSTargetStreamer(MCStreamer &S);
};
} // namespace llvm

#endif