include "llvm/Target/Target.td"


//=---------------------------------------=//
// Register File Description
//=---------------------------------------=//
include "GASSRegisterInfo.td"

//=---------------------------------------=//
// Instruction Description
//=---------------------------------------=//
include "GASSSchedule.td"
include "GASSInstrInfo.td"

def SM70 : SubtargetFeature<"sm_70", "SmVersion", "70", "Target SM 7.0">;
def SM75 : SubtargetFeature<"sm_75", "SmVersion", "75", "Target SM 7.5">;
def SM80 : SubtargetFeature<"sm_80", "SmVersion", "80", "Target SM 8.0">;

// Processes
class Proc<string Name, list<SubtargetFeature> Features>
  : Processor<Name, NoItineraries, Features>;

// SchedModel
include "GASSSchedSM70.td"

// ProcessorModel = Processor + SchedModel
def : ProcessorModel<"sm_70", SM70Model, [SM70]>;
// Turing can reuse Volta's Schedule Model
def : ProcessorModel<"sm_75", SM70Model, [SM75]>;
def : ProcessorModel<"sm_80", SM70Model, [SM80]>;

def GASSInstrInfo : InstrInfo {}

def GASS : Target {
  let InstructionSet = GASSInstrInfo;
}