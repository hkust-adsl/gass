#include "LiveBarRange.h"

using namespace llvm;

bool LiveBarRange::overlaps(SlotIndex S, SlotIndex E) const {
  assert(S <= E && "Invalid range");
  return !(end <= S || start >= E);
}

bool LiveBarRange::overlaps(const LiveBarRange &Other) const {
  return overlaps(Other.beginIndex(), Other.endIndex());
}