#include "GASSAsmStreamer.h"
#include "llvm/Support/FormattedStream.h"

using namespace llvm;

GASSTargetAsmStreamer::GASSTargetAsmStreamer(MCStreamer &S, 
                                             formatted_raw_ostream &OS) 
  : GASSTargetStreamer(S), OS(OS) {}

void GASSTargetAsmStreamer::emitDwarfFileDirective(StringRef Directive) {
  // TODO: fill this.
}

void GASSTargetAsmStreamer::emitAttributes(unsigned SmVersion) {
  OS << ".target sm_" << SmVersion;
}