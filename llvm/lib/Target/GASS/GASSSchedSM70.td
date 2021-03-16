// Each subtarget has a SchedModel
// Ref: RISCVSchedSiFive7.td
// 1. Basic Model
def SM70Model : SchedMachineModel {
  let IssueWidth = 1; // In-order
  let MicroOpBufferSize = 0; // AArch64 = 0; AMDGPU (SI) = 1;

  let CompleteModel = false; // Required by postRAScheduelr
}

// 2. Processor resources
let SchedModel = SM70Model in {
def SM70UnitALU   : ProcResource<1> { let BufferSize = 0; }
def SM70UnitLDST  : ProcResource<1> { let BufferSize = 5; }
def SM70UnitFP32ALU : ProcResource<1> { let BufferSize = 0; }
def SM70UnitFP64ALU : ProcResource<1> { let BufferSize = 0; }
def SM70UnitSFU : ProcResource<1> { let BufferSize = 0; }
def SM70UintTensorCore : ProcResource<1> { let BufferSize = 0; }
// Other


// WriteRes
// 4. Map processor resources to SchedWrites
// Integer arithmetic and logic
def : WriteRes<WriteINT, [SM70UnitALU]> { let Latency = 5; }
def : WriteRes<WriteSHIFT, [SM70UnitALU]> { let Latency = 6; }
def : WriteRes<WriteLOP3, [SM70UnitALU]> { let Latency = 8; } // Maybe 6?
def : WriteRes<WriteISETP, [SM70UnitALU]> { let Latency = 12; }
def : WriteRes<WritePRMT, [SM70UnitALU]> { let Latency = 9; }
// Uses FP32 pipeline
def : WriteRes<WriteIMUL, [SM70UnitFP32ALU]> { let Latency = 5; }
def : WriteRes<WriteIMULWIDE, [SM70UnitFP32ALU]> { let Latency = 8; }

// Single precision.
def : WriteRes<WriteFP32, [SM70UnitFP32ALU]> { let Latency = 5; }

// Double precision.
def : WriteRes<WriteFP64, [SM70UnitFP64ALU]> { let Latency = 8; }

// SFU (e.g., F2F, ...)
def : WriteRes<WriteF2F, [SM70UnitSFU]> { let Latency = 14; } // TODO: check this

// TensorCore
def : WriteRes<WriteHMMA884, [SM70UintTensorCore]> { let Latency = 14; }

// Memory
def : WriteRes<WriteLDS, [SM70UnitLDST]> { let Latency = 20; }
def : WriteRes<WriteSTS, [SM70UnitLDST]> { let Latency = 20; }
def : WriteRes<WriteLDG, [SM70UnitLDST]> { let Latency = 300; }
def : WriteRes<WriteSTG, [SM70UnitLDST]> { let Latency = 100; } // TODO: lat?

// Refine Basic Model
// 1. InstRW is used to refine scheduling info, override the target defaults
// Miscellaneous
def : InstRW<[WriteINT], (instrs COPY)>;
// 2. WriteSequence (?)
}