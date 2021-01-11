// Each subtarget has a SchedModel
// 1. Basic Model
def GASSSchedMachineModel : SchedMachineModel {
  let IssueWidth = 1;
  let MicroOpBufferSize = 0; // AArch64 = 0; AMDGPU (SI) = 1;

  let CompleteModel = true;
}

// 2. Processor resources
def GASSUnitALU : ProcResource</*NumUnit*/1> { let BufferSize = 0; }
def GASSUnitLdst : ProcResource<1> { let BufferSize = 5; }
def GASSUnitFPALU : ProcResource<1> { let BufferSize = 0; }
// Other

// What's this?
// 3. SchedWrite(s) (& SchedRead(s))?
def WriteI : SchedWrite; 

// WriteRes
// 4. Map processor resources to SchedWrites
let SchedModel = GASSSchedMachineModel in {

def : WriteRes<WriteI, [GASSUnitALU]> { let Latency = 4; }

}

// Refine Basic Model
// 1. InstRW is used to refine scheduling info, override the target defaults

// 2. WriteSequence (?)