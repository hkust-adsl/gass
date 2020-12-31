#include "llvm/CodeGen/GlobalISel/InstructionSelector.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelectorImpl.h"

#define DEBUG_TYPE "gass-isel"

using namespace llvm;

namespace {

class GASSInstructionSelector : public InstructionSelector {
public:
  GASSInstructionSelector(const GASSTargetMachine &TM,
                          const GASSSubtarget &STI);
  
  bool select(MachineInstr &I) override;
  static const char *getName() { return DEBUG_TYPE; }

private:
  // Tablegen auto-generated
  bool selectImpl(MachineInstr &I, CodeGenCoverage &CoverageInfo) const;

#define GET_GLOBALISEL_PREDICATES_DECL
#include "GASSGenGlobalISel.inc"
#undef GET_GLBOALISEL_PREDICATES_DECL
#define GET_GLOBALISEL_TEMPORARIES_DECL
#include "GASSGenGlobalISel.inc"
#undef GET_GLBAOLISEL_TEMPORARIES_DECL
};

} // end anonymous namespace

#define GET_GLOBALISEL_IMPL
#include "GASSGenGlobalISel.inc"
#undef GET_GLOBALISEL_IMPL

GASSInstructionSelector::GASSInstructionSelector(
  const GASSTargetMachine &TM, const GASSSubtarget &STI)
  : InstructionSelector(), STI(STI),
    #define GET_GLOBALISEL_PREDICATES_INIT
    #include "GASSGenGlobalISel.inc"
    #undef GET_GLBOALISEL_PREDICATES_INIT
    #define GET_GLOBALISEL_TEMPORARIES_INIT
    #include "GASSGenGlobalISel.inc"
    #undef GET_GLBAOLISEL_TEMPORARIES_INIT
{}

bool GASSInstructionSelector::select(MachineInstr &I) {
  if (selectImpl(I, *CoverageInfo))
    return true;

  return false;
}

namespace llvm {
InstructionSelector *
createGASSInstructionSelector(const GASSTargetMachine &TM,
                              GASSSubtarget &Subtarget) {
  return new GASSInstructionSelector(TM, Subtarget);
};
}