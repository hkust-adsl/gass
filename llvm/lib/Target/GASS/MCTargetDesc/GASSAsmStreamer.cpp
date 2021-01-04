#include "GASSAsmStreamer.h"

using namespace llvm;

GASSTargetAsmStreamer::GASSTargetAsmStreamer(MCStreamer &S, 
                                             formatted_raw_ostream &OS) 
  : GASSTargetStreamer(S), OS(OS) {}

void GASSTargetAsmStreamer::emitDwarfFileDirective(StringRef Directive) {
  // TODO: fill this.
}