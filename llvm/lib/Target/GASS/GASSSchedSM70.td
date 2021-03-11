// Each subtarget has a SchedModel
// 1. Basic Model
def SM70Model : SchedMachineModel {
  let IssueWidth = 1; // In-order
  let MicroOpBufferSize = 0; // AArch64 = 0; AMDGPU (SI) = 1;

  let CompleteModel = true;
}

// 2. Processor resources
def SM70UnitALU   : ProcResource<1> { let BufferSize = 0; }
def SM70UnitLDST  : ProcResource<1> { let BufferSize = 5; }
def SM70UnitFP32ALU : ProcResource<1> { let BufferSize = 0; }
def SM70UnitFP64ALU : ProcResource<1> { let BufferSize = 0; }
def SM70UintTensorCore : ProcResource<1> { let BufferSize = 0; }
// Other



// WriteRes
// 4. Map processor resources to SchedWrites
let SchedModel = SM70Model in {

def : WriteRes<WriteINT, [SM70UnitALU]> { let Latency = 5; }
def : WriteRes<WriteISETP, [SM70UnitALU]> { let Latency = 12; }
def : WriteRes<WriteFP32, [SM70UnitFP32ALU]> { let Latency = 5; }
def : WriteRes<WriteFP64, [SM70UnitFP64ALU]> { let Latency = 8; }
def : WriteRes<WriteTC, [SM70UintTensorCore]> { let Latency = 8; }
def : WriteRes<WriteLD, [SM70UnitLDST]> { let Latency = 20; }
def : WriteRes<WriteST, [SM70UnitLDST]> { let Latency = 20; }

// Refine Basic Model
// 1. InstRW is used to refine scheduling info, override the target defaults
// Miscellaneous
def : InstRW<[WriteINT], (instrs COPY)>;
// 2. WriteSequence (?)
}

