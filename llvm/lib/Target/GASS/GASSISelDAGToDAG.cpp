#include "GASSISelDAGToDAG.h"

using namespace llvm;

#define DEBUG_TYPE "gass-isel"

void GASSDAGToDAGISel::Select(SDNode *N) {
  // Tablegen'erated
  SelectCode(N);
}