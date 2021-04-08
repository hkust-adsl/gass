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
def SM70UnitLDST  : ProcResource<1> { let BufferSize = 0; /*5*/ }
def SM70UnitFP32ALU : ProcResource<1> { let BufferSize = 0; }
def SM70UnitFP64ALU : ProcResource<1> { let BufferSize = 0; }
def SM70UnitSFU : ProcResource<1> { let BufferSize = 0; }
def SM70UintTensorCore : ProcResource<1> { let BufferSize = 0; }
// Other


// WriteRes
// 4. Map processor resources to SchedWrites
// Integer arithmetic and logic
def : WriteRes<WriteINT, [SM70UnitALU]> { let Latency = 6; 
                                          let ResourceCycles = [2]; }
def : WriteRes<WriteSHIFT, [SM70UnitALU]> { let Latency = 8; 
                                            let ResourceCycles = [2]; }
def : WriteRes<WriteLOP3, [SM70UnitALU]> { let Latency = 8; // Maybe 6?
                                           let ResourceCycles = [2]; } 
def : WriteRes<WriteISETP, [SM70UnitALU]> { let Latency = 12; 
                                            let ResourceCycles = [2]; }
def : WriteRes<WritePRMT, [SM70UnitALU]> { let Latency = 10; 
                                           let ResourceCycles = [2]; }
// Uses FP32 pipeline
def : WriteRes<WriteIMUL, [SM70UnitFP32ALU]> { let Latency = 5; 
                                               let ResourceCycles = [2]; }
def : WriteRes<WriteIMULWIDE, [SM70UnitFP32ALU]> { let Latency = 8; 
                                                   let ResourceCycles = [4]; }

// Single precision.
def : WriteRes<WriteFP32, [SM70UnitFP32ALU]> { let Latency = 5; 
                                               let ResourceCycles = [2]; }

// Double precision.
def : WriteRes<WriteFP64, [SM70UnitFP64ALU]> { let Latency = 8; // TODO: verify this
                                               let ResourceCycles = [4]; }

// SFU (e.g., F2F, ...)
def : WriteRes<WriteF2F, [SM70UnitSFU]> { let Latency = 10; // TODO: verify this
                                          let ResourceCycles = [8]; }

// TensorCore
def : WriteRes<WriteHMMA884, [SM70UintTensorCore]> { let Latency = 10; // TODO: verify this
                                                     let ResourceCycles = [2]; }

// Memory
// shared memory (4 cycles for LDS.32 (w/o bank conflict))
def : WriteRes<WriteLDS, [SM70UnitLDST]> { let Latency = 20; 
                                           let ResourceCycles = [4]; }
def : WriteRes<WriteSTS, [SM70UnitLDST]> { let Latency = 20; 
                                           let ResourceCycles = [4]; }
def : WriteRes<WriteLDS64, [SM70UnitLDST]> { let Latency = 20; 
                                             let ResourceCycles = [8]; }
def : WriteRes<WriteSTS64, [SM70UnitLDST]> { let Latency = 20; 
                                             let ResourceCycles = [8]; }
def : WriteRes<WriteLDS128, [SM70UnitLDST]> { let Latency = 20; 
                                              let ResourceCycles = [16]; }
def : WriteRes<WriteSTS128, [SM70UnitLDST]> { let Latency = 20; 
                                              let ResourceCycles = [16]; }
// global memory
def : WriteRes<WriteLDG, [SM70UnitLDST]> { let Latency = 300; 
                                           // TODO: verify this.
                                           let ResourceCycles = [40]; }
def : WriteRes<WriteSTG, [SM70UnitLDST]> { let Latency = 100; 
                                           let ResourceCycles = [20]; } // TODO: lat?

// Refine Basic Model
// 1. InstRW is used to refine scheduling info, override the target defaults
// Miscellaneous
def : InstRW<[WriteINT], (instrs COPY)>;
// 2. WriteSequence (?)
}