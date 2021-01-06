include "llvm/Target/Target.td"

include "GASSRegisterInfo.td"
include "GASSInstrInfo.td"
// include "GASSRegisterBanks.td"

def SM70 : SubtargetFeature<"sm_70", "SmVersion", "70", "Target SM 7.0">;
def SM75 : SubtargetFeature<"sm_75", "SmVersion", "75", "Target SM 7.5">;
def SM80 : SubtargetFeature<"sm_80", "SmVersion", "80", "Target SM 8.0">;

// Processes
class Proc<string Name, list<SubtargetFeature> Features>
  : Processor<Name, NoItineraries, Features>;

def : Proc<"sm_70", [SM70]>;
def : Proc<"sm_75", [SM75]>;
def : Proc<"sm_80", [SM80]>;

def GASSInstrInfo : InstrInfo {}

def GASS : Target {
  let InstructionSet = GASSInstrInfo;
}