#include "GASSSubtarget.h"
#include "GASSTargetMachine.h"
#include "GASSRegisterBankInfo.h"

#include "llvm/CodeGen/GlobalISel/InstructionSelector.h"
#include "llvm/CodeGen/GlobalISel/InstructionSelectorImpl.h"

#define DEBUG_TYPE "gass-isel"

using namespace llvm;

// What's this?
#define GET_GLOBALISEL_PREDICATE_BITSET
#include "GASSGenGlobalISel.inc"
#undef GET_GLOBALISEL_PREDICATE_BITSET

namespace {

class GASSInstructionSelector : public InstructionSelector {
public:
  GASSInstructionSelector(const GASSTargetMachine &TM,
                          const GASSSubtarget &STI, 
                          const GASSRegisterBankInfo &RBI);
  
  bool select(MachineInstr &I) override;
  static const char *getName() { return DEBUG_TYPE; }

private:
  // Tablegen auto-generated
  bool selectImpl(MachineInstr &I, CodeGenCoverage &CoverageInfo) const;

  // Required by Tablegen'd
  const GASSSubtarget &STI;
  const GASSInstrInfo &TII;
  const GASSRegisterInfo &TRI;
  const GASSRegisterBankInfo &RBI;

#define GET_GLOBALISEL_PREDICATES_DECL
#include "GASSGenGlobalISel.inc"
#undef GET_GLOBALISEL_PREDICATES_DECL
#define GET_GLOBALISEL_TEMPORARIES_DECL
#include "GASSGenGlobalISel.inc"
#undef GET_GLOBALISEL_TEMPORARIES_DECL
};

} // end anonymous namespace

#define GET_GLOBALISEL_IMPL
#include "GASSGenGlobalISel.inc"
#undef GET_GLOBALISEL_IMPL

GASSInstructionSelector::GASSInstructionSelector(
  const GASSTargetMachine &TM, const GASSSubtarget &STI,
  const GASSRegisterBankInfo &RBI)
  : InstructionSelector(), STI(STI), TII(*STI.getInstrInfo()),
    TRI(*STI.getRegisterInfo()), RBI(RBI),
    #define GET_GLOBALISEL_PREDICATES_INIT
    #include "GASSGenGlobalISel.inc"
    #undef GET_GLOBALISEL_PREDICATES_INIT
    #define GET_GLOBALISEL_TEMPORARIES_INIT
    #include "GASSGenGlobalISel.inc"
    #undef GET_GLOBALISEL_TEMPORARIES_INIT
{}

bool GASSInstructionSelector::select(MachineInstr &I) {
  if (selectImpl(I, *CoverageInfo))
    return true;

  return false;
}

namespace llvm {
InstructionSelector *
createGASSInstructionSelector(const GASSTargetMachine &TM,
                              const GASSSubtarget &Subtarget,
                              const GASSRegisterBankInfo &RBI) {
  return new GASSInstructionSelector(TM, Subtarget, RBI);
}
}