#include "GASSMCTargetDesc.h"
#include "GASSELFStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"

using namespace llvm;

GASSTargetELFStreamer::GASSTargetELFStreamer(MCStreamer &S,
                                             const MCSubtargetInfo &STI)
  : GASSTargetStreamer(S), STI(STI) {}

MCELFStreamer &GASSTargetELFStreamer::getStreamer() {
  return static_cast<MCELFStreamer &>(Streamer);
}

// Override virtual functions
void GASSTargetELFStreamer::emitAttributes(unsigned SmVersion) {
  MCAssembler &MCA = getStreamer().getAssembler();

  unsigned EFlags = MCA.getELFHeaderEFlags();
  
  EFlags |= (SmVersion);
  EFlags |= (SmVersion) << 16;

  MCA.setELFHeaderEFlags(EFlags);
}