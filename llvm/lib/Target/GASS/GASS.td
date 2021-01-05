include "llvm/Target/Target.td"

include "GASSRegisterInfo.td"
include "GASSInstrInfo.td"
// include "GASSRegisterBanks.td"

def SM70 : SubtargetFeature<"sm_70", "SmVersion", "70", "Target SM 7.0">;

// Processes
class Proc<string Name, list<SubtargetFeature> Features>
  : Processor<Name, NoItineraries, Features>;

def : Proc<"sm_70", [SM70]>;

def GASSInstrInfo : InstrInfo {}

def GASS : Target {
  let InstructionSet = GASSInstrInfo;
}