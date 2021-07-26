#include "GASS.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/CodeGen/LiveInterval.h"
#include "llvm/CodeGen/LivePhysRegs.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/SlotIndexes.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/Register.h"
#include "llvm/InitializePasses.h"

using namespace llvm;

#define PASS_NAME "GASS If Conversion"
#define DEBUG_TYPE "gass-ifcvt"

STATISTIC(NumTrianglesIfCvt, "Number of triangles");
STATISTIC(NumDiamondsIfCvt,  "Number of diamonds");

namespace {
// Ref: SSAIfConv
// GASSIfCvt can convert both triangles and diamonds (Not Yet):
//
//   Triangle: Head              Diamond: Head (Not Yet)
//              | \                       /  \_
//              |  \                     /    |
//              |  [TF]BB              FBB    TBB
//              |  /                     \    /
//              | /                       \  /
//             Tail                       Tail
//
class GASSIfCvt {
  const TargetInstrInfo *TII = nullptr;
  const TargetRegisterInfo *TRI = nullptr;
  LiveIntervals *LIS = nullptr;
  MachineRegisterInfo *MRI = nullptr;
public:
  MachineBasicBlock *Head;
  MachineBasicBlock *Tail;
  MachineBasicBlock *TBB;
  MachineBasicBlock *FBB;

  bool isTriangle() const { return TBB == Tail || FBB == Tail; }

private:
  /// The branch condition determined by analyzeBranch.
  SmallVector<MachineOperand, 4> Cond;

  /// Insertion point in Head for speculatively executed instructions form TBB
  /// and FBB (?)
  MachineBasicBlock::iterator InsertionPoint;

  /// Predicate all instructions of the basic block
  void predicateBlock(MachineBasicBlock *MBB, bool ReversePredicate);

  /// Find a valid insertion point in Head.
  bool findInsertionPoint();

  void fixLiveRange(const SlotIndex &Origin, const SlotIndex &New);

public:
  /// If the sub-CFG headed by MBB can be if-converted, 
  /// initialize and return true.
  bool canConvertIf(MachineBasicBlock *MBB);

  /// If-convert the last block passed to canConvertIf, assuming it's possible
  /// @param RemovedBlocks Erased blocks
  void convertIf(SmallVectorImpl<MachineBasicBlock *> &RemovedBlocks);

  /// Initialize per-function data structures
  void initialize(MachineFunction &MF, LiveIntervals *LIS) {
    TII = MF.getSubtarget().getInstrInfo();
    TRI = MF.getSubtarget().getRegisterInfo();
    MRI = &MF.getRegInfo();
    this->LIS = LIS;
  }
};

} // anonymous namespace

//==------------------------------------------------------------------------==//
// GASSIfCvt
//==------------------------------------------------------------------------==//
void GASSIfCvt::predicateBlock(MachineBasicBlock *MBB, bool ReversePredicate) {
  SmallVector<MachineOperand, 4> Condition = Cond;
  if (ReversePredicate)
    TII->reverseBranchCondition(Condition);
  // Terminators don't need to be predicated as they will be removed.
  for (MachineBasicBlock::iterator I = MBB->begin(),
                                   E = MBB->getFirstTerminator();
       I != E; ++I) {
    if (I->isDebugInstr())
      continue;
    TII->PredicateInstruction(*I, Condition);
  }
}

bool GASSIfCvt::canConvertIf(MachineBasicBlock *MBB) {
  Head = MBB;
  TBB = FBB = Tail = nullptr;

  if (Head->succ_size() != 2)
    return false;
  MachineBasicBlock *Succ0 = Head->succ_begin()[0];
  MachineBasicBlock *Succ1 = Head->succ_begin()[1];

  // Canonicalize so Succ0 has MBB as its single predecessor.
  if (Succ0->pred_size() != 1)
    std::swap(Succ0, Succ1);

  if (Succ0->pred_size() != 1 || Succ0->succ_size() != 1)
    return false;

  Tail = Succ0->succ_begin()[0];

  // This is not a triangle.
  if (Tail != Succ1) {
    // Check for a diamond.
    if (Succ1->pred_size() != 1 || Succ1->succ_size() != 1 ||
        Succ1->succ_begin()[0] != Tail)
      return false;
    // Is a diamond
  }

  // The branch we're looking to eliminate must be analyzable.
  Cond.clear();
  if (TII->analyzeBranch(*Head, TBB, FBB, Cond)) {
    LLVM_DEBUG(dbgs() << "Branch not analyzable.\n");
    return false;
  }

  // analyzeBranch doesn't set FBB on a fall-through branch.
  // Make sure it is always set.
  FBB = (TBB == Succ0)? Succ1 : Succ0;

  // Try to find a valid insertion point.
  InsertionPoint = Head->getFirstTerminator();
  
  if (isTriangle())
    ++NumTrianglesIfCvt;
  else 
    ++NumDiamondsIfCvt;

  return true;
}

void GASSIfCvt::convertIf(SmallVectorImpl<MachineBasicBlock *> &RemovedBlocks) {
  assert(Head && Tail && TBB && FBB && "Call canConvertIf first.");
  assert(isTriangle() && "Only support Triangle for now");

  SlotIndex Origin, New; // Update Segment
  MachineBasicBlock::iterator PreInsertionPoint = std::prev(InsertionPoint);
  if (TBB != Tail) {
    predicateBlock(TBB, /*ReversePredicate=*/false);
    Origin = LIS->getMBBStartIdx(TBB);
    Head->splice(InsertionPoint, TBB, TBB->begin(), TBB->getFirstTerminator());
    New = LIS->getInstructionIndex(*std::next(PreInsertionPoint));
    fixLiveRange(Origin, New);
  }
  if (FBB != Tail) {
    predicateBlock(FBB, /*ReversePredicate=*/true);
    Origin = LIS->getMBBStartIdx(FBB);
    Head->splice(InsertionPoint, FBB, FBB->begin(), FBB->getFirstTerminator());
    New = LIS->getInstructionIndex(*std::next(PreInsertionPoint));
    fixLiveRange(Origin, New);
  }

  // TODO: Are there extra Tail predecessors?
  bool ExtraPreds = Tail->pred_size() != 2;
  
  // Fixup the CFG
  // TODO: review this
  Head->removeSuccessor(TBB);
  Head->removeSuccessor(FBB, true);
  if (TBB != Tail)
    TBB->removeSuccessor(Tail, true);
  if (FBB != Tail)
    FBB->removeSuccessor(Tail, true);
  
  // Fix up Head's terminators
  // It should become a single branch or a fallthrough
  DebugLoc HeadDL = Head->getFirstTerminator()->getDebugLoc();
  TII->removeBranch(*Head); // TODO: check this

  // Erase the now empty conditional blocks. It is likely that Head can fall
  // through to Tail, and we can join the two blocks.
  if (TBB != Tail) {
    RemovedBlocks.push_back(TBB);
    TBB->eraseFromParent();
  }
  if (FBB != Tail) {
    RemovedBlocks.push_back(FBB);
    FBB->eraseFromParent();
  }

  assert(Head->succ_empty() && "Additional head successors?");
  if (!ExtraPreds && Head->isLayoutSuccessor(Tail)) {
    // Splice Tail onto the end of Head.
    LLVM_DEBUG(dbgs() << "Joining tail " << printMBBReference(*Tail)
                      << " into head " << printMBBReference(*Head) << '\n');
    Head->splice(Head->end(), Tail,
                     Tail->begin(), Tail->end());
    Head->transferSuccessorsAndUpdatePHIs(Tail);
    RemovedBlocks.push_back(Tail);
    Tail->eraseFromParent();
  } else {
    // We need a branch to Tail, let code placement work it out later.
    LLVM_DEBUG(dbgs() << "Converting to unconditional branch.\n");
    SmallVector<MachineOperand, 0> EmptyCond;
    TII->insertBranch(*Head, Tail, nullptr, EmptyCond, HeadDL);
    Head->addSuccessor(Tail);
  }
  LLVM_DEBUG(dbgs() << *Head);
}

// Any LiveRange that ends with Origin should be replaced by New
void GASSIfCvt::fixLiveRange(const SlotIndex &Origin, const SlotIndex &New) {
  // Ref: LiveIntervals::repairIntervalsInRange
  for (size_t I = 0, E = MRI->getNumVirtRegs(); I < E; ++I) {
    Register Reg = Register::index2VirtReg(I);
    if (!Reg.isVirtual())
      continue;
    if (!LIS->hasInterval(Reg))
      continue;
    LiveInterval &LI = LIS->getInterval(Reg);
    for (size_t i=0; i<LI.segments.size(); ++i) {
      LiveInterval::Segment &Seg = LI.segments[i];
      if (Seg.end == Origin) {
        Seg.end = New;
        LI.verify();
        // FIXME: this only works for some special cases
        if (++i < LI.segments.size()) { // preventing following passes to mark reg as dead
          if (LI.segments[i].start == New) {
            MachineInstr *NextMI = LIS->getInstructionFromIndex(New);
            MachineInstrBuilder MIB(*NextMI->getMF(), NextMI);
            MIB.addUse(Reg, RegState::Implicit);
          }
        }          
        break;
      }
    }
  }
}

//==------------------------------------------------------------------------==//
// GASSIfConversion
//==------------------------------------------------------------------------==//
namespace {
class GASSIfConversion : public MachineFunctionPass {
  const TargetInstrInfo *TII = nullptr;
  const TargetRegisterInfo *TRI = nullptr;
  MachineDominatorTree *DomTree = nullptr;
  MachineLoopInfo *Loops = nullptr;
  LiveIntervals *LIS = nullptr;
  MachineRegisterInfo *MRI = nullptr;

  GASSIfCvt IfCvt;

  friend class LiveIntervals;
public:
  static char ID;

  GASSIfConversion() : MachineFunctionPass(ID) {
    initializeGASSIfConversionPass(*PassRegistry::getPassRegistry());
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override;

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return PASS_NAME;
  }

private:
  bool tryConvertIf(MachineBasicBlock *);
  bool shouldConvertIf();


};

char GASSIfConversion::ID = 0;
} // anonymous namespace

INITIALIZE_PASS_BEGIN(GASSIfConversion, DEBUG_TYPE, PASS_NAME, false, false)
INITIALIZE_PASS_DEPENDENCY(MachineDominatorTree)
INITIALIZE_PASS_DEPENDENCY(LiveIntervals)
INITIALIZE_PASS_END(GASSIfConversion, DEBUG_TYPE, PASS_NAME, false, false)

void GASSIfConversion::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<MachineDominatorTree>();
  AU.addPreserved<MachineDominatorTree>();
  AU.addRequired<MachineLoopInfo>();
  AU.addPreserved<MachineLoopInfo>();
  AU.addRequired<LiveIntervals>();
  MachineFunctionPass::getAnalysisUsage(AU);
}

/// Apply cost model and heuristics.
/// Return true if the conversion is a good idea.
bool GASSIfConversion::shouldConvertIf() {
  // TODO: update this.
  // Could change to simple heuristic: e.g., < 15 instructions.
  return true;
}

/// static helpers
static void updateDomTree(MachineDominatorTree *DomTree, 
                          const GASSIfCvt &IfCvt, 
                          ArrayRef<MachineBasicBlock*> Removed) {
  // convertIf can remove TBB, FBB, and Tail can be merged into Head
  // TBB and FBB should not dominate any blocks
  // Tail children should be transforred to Head.
  MachineDomTreeNode *HeadNode = DomTree->getNode(IfCvt.Head);
  for (MachineBasicBlock *B : Removed) {
    MachineDomTreeNode *Node = DomTree->getNode(B);
    assert(Node != HeadNode && "Cannot erase the head node");
    while (Node->getNumChildren()) {
      assert(Node->getBlock() == IfCvt.Tail && "Unexpected children");
      DomTree->changeImmediateDominator(Node->back(), HeadNode);
    }
    DomTree->eraseNode(B);
  }
}

static void updateLoops(MachineLoopInfo *Loops,
                        ArrayRef<MachineBasicBlock*> Removed) {
  if (!Loops)
    return;
  for (MachineBasicBlock *B : Removed)
    Loops->removeBlock(B);
}

/// Attempt repeated if-conversion on MBB, return true if successful
bool GASSIfConversion::tryConvertIf(MachineBasicBlock *MBB) {
  bool Changed = false;
  while (IfCvt.canConvertIf(MBB) && shouldConvertIf()) {
    SmallVector<MachineBasicBlock*, 4> RemovedBlocks;
    IfCvt.convertIf(RemovedBlocks);
    Changed = true;
    updateDomTree(DomTree, IfCvt, RemovedBlocks);
    updateLoops(Loops, RemovedBlocks);
  }
  return Changed;
}

bool GASSIfConversion::runOnMachineFunction(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "************** GASS IF-CONVERSION ****************"
                    << "************** Function: " << MF.getName() << "\n");
  
  const TargetSubtargetInfo &STI = MF.getSubtarget();

  TII = STI.getInstrInfo();
  TRI = STI.getRegisterInfo();
  DomTree = &getAnalysis<MachineDominatorTree>();
  Loops = getAnalysisIfAvailable<MachineLoopInfo>();
  LIS = &getAnalysis<LiveIntervals>();
  MRI = &MF.getRegInfo();

  bool Changed = false;
  IfCvt.initialize(MF, LIS);

  // TODO: check this.
  // Visit blocks in dominator tree post-order.
  // The post-order enables nested if-conversion in a single pass. (?)
  for (auto DomNode : post_order(DomTree))
    if (tryConvertIf(DomNode->getBlock()))
      Changed = true;

  // No longer in SSA form, we don't need IMPLICIT_DEF
  for (MachineBasicBlock &MBB : MF) {
    std::vector<MachineInstr*> ToDelete;
    for (MachineInstr &MI : MBB)
      if (MI.getOpcode() == TargetOpcode::IMPLICIT_DEF)
        ToDelete.push_back(&MI);
    for (MachineInstr *I : ToDelete)
      MBB.erase_instr(I);
  }

  return Changed;
}

//==------------------------------------------------------------------------==//
// public interface
//==------------------------------------------------------------------------==//
FunctionPass *llvm::createGASSIfConversionPass() {
  return new GASSIfConversion();
}