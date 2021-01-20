//==--------------------------------------------------------------==//
//
// Observed cubin file format:
//   header.flags = arch | (arch << 16) (e.g., 70 (sm_70))
//
// symtab:
//   //
// 
// .nv.info. section
// 
//
// for each kernel, sections:
//   a. .nv.info.{name}
//   b. .nv.constant0.{name}
//   c. .text.{name}
//   d. .nv.shared.{name} (optional)
//   
// for each kernel, we need info:
//   name
//   code data
//   #registers
//   #bar
//   smem_size
//   const_size
//   exit_offsets
//   param_sizes
//   
//==--------------------------------------------------------------==//

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