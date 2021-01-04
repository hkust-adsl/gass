#include "GASSTargetStreamer.h"

using namespace llvm;

GASSTargetStreamer::GASSTargetStreamer(MCStreamer &S)
  : MCTargetStreamer(S) {}