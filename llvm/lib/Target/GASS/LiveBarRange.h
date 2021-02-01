//===----------------------------------------------------------------------===//
//
// LiveRange in "llvm/CodeGen/LiveInterval.h" is too complex and it seems that 
// LiveRange cannot meet our requirement. LiveRange has no constructor from 
// (start, end) index tuple.
// So we provide our own LiveRange for Barriers (LiveBarRange)
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIB_TARGEG_GASS_LIVEBARRANGE_H
#define LLVM_LIB_TARGEG_GASS_LIVEBARRANGE_H

#include "llvm/CodeGen/SlotIndexes.h"

namespace llvm {
class LiveBarRange {
  // Ranges are rendered as [start, end)
  SlotIndex start; // Start point of the interval (inclusive)
  SlotIndex end;   // End point of the interval (exclusive)
public:
  LiveBarRange(SlotIndex S, SlotIndex E)
    : start(S), end(E) {}
  
  bool contains(SlotIndex I) const { return start <= I && I < end; }

  /// Return true if the given interval, [S, E), is covered by this segment.
  bool containsInterval(SlotIndex S, SlotIndex E) const {
    assert((S < E) && "Backwards interval?");
    return (start <= S && S < end) && (start < E && E <= end);
  }

  bool operator<(const LiveBarRange &Other) const {
    return std::tie(start, end) < std::tie(Other.start, Other.end);
  }
  bool operator==(const LiveBarRange &Other) const {
    return start == Other.start && end == Other.end;
  }

  bool operator!=(const LiveBarRange &Other) const {
    return !(*this == Other);
  }

  // accessor
  SlotIndex beginIndex() const { return start; }
  SlotIndex endIndex() const { return end; }

  // if overlaps
  bool overlaps(const LiveBarRange &Other) const;
  bool overlaps(SlotIndex S, SlotIndex E) const;
  
  // // if complete covers
  // bool covers(const LiveBarRange &Other) const;
  // bool covers(SlotIndex S, SlotIndex E) const;

  bool expireAt(SlotIndex Index) const {
    return Index >= end || Index < start;
  }
  bool liveAt(SlotIndex Index) const {
    return start <= Index && Index < end;
  }

  // join
  void join(LiveBarRange &Other);

  // merge
  void merge(LiveBarRange &Other);

  void dump() const;
};
} // namespace llvm

#endif