// Each subtarget has a SchedModel
// Ref: RISCVSchedSiFive7.td
// 1. Basic Model
def SM80Model : SchedMachineModel {
  let IssueWidth = 1; // In-order
  let MicroOpBufferSize = 0; // AArch64 = 0; AMDGPU (SI) = 1;

  let CompleteModel = false; // Required by postRAScheduelr
}

// 2. Processor resources
let SchedModel = SM80Model in {
def SM80UnitALU   : ProcResource<1> { let BufferSize = 0; }
def SM80UnitLDST  : ProcResource<1> { let BufferSize = 0; /*5*/ }
def SM80UnitFP32ALU : ProcResource<1> { let BufferSize = 0; }
def SM80UnitFP64ALU : ProcResource<1> { let BufferSize = 0; }
def SM80UnitSFU : ProcResource<1> { let BufferSize = 0; }
def SM80UnitTensorCore : ProcResource<1> { let BufferSize = 0; }
// Other


// WriteRes
// 4. Map processor resources to SchedWrites
// Integer arithmetic and logic
def : WriteRes<WriteINT, [SM80UnitALU]> { let Latency = 8; 
                                          let ResourceCycles = [2]; }
def : WriteRes<WriteSHIFT, [SM80UnitALU]> { let Latency = 8; 
                                            let ResourceCycles = [2]; }
def : WriteRes<WriteLOP3, [SM80UnitALU]> { let Latency = 8; // Maybe 6?
                                           let ResourceCycles = [2]; } 
def : WriteRes<WriteISETP, [SM80UnitALU]> { let Latency = 12; 
                                            let ResourceCycles = [2]; }
def : WriteRes<WritePRMT, [SM80UnitALU]> { let Latency = 10; 
                                           let ResourceCycles = [2]; }
def : WriteRes<WriteCSR, [SM80UnitALU]> { let Latency = 5; // 4 for Turing & Ampere
                                          let ResourceCycles = [6]; }
def : WriteRes<WriteNOP, []> { let Latency = 1; let ResourceCycles = []; } 
// Uses FP32 pipeline
def : WriteRes<WriteIMUL, [SM80UnitFP32ALU]> { let Latency = 8; 
                                               let ResourceCycles = [2]; }
def : WriteRes<WriteIMULWIDE, [SM80UnitFP32ALU]> { let Latency = 8; 
                                                   let ResourceCycles = [4]; }

// Single precision.
def : WriteRes<WriteFP32, [SM80UnitFP32ALU]> { let Latency = 8; 
                                               let ResourceCycles = [2]; }
def : WriteRes<WriteFMNMX, [SM80UnitFP32ALU]> { let Latency = 8;
                              /* verify this */ let ResourceCycles = [2]; }

// Double precision.
def : WriteRes<WriteFP64, [SM80UnitFP64ALU]> { let Latency = 8; // TODO: verify this
                                               let ResourceCycles = [4]; }

// SFU (e.g., F2F, MUFU)
def : WriteRes<WriteF2F, [SM80UnitSFU]> { let Latency = 10; // TODO: verify this
                                          let ResourceCycles = [8]; }
def : WriteRes<WriteMUFU, [SM80UnitSFU]> { let Latency = 10; // TODO: verify this
                                           let ResourceCycles = [8]; }

// TensorCore
def : WriteRes<WriteHMMA884, [SM80UnitTensorCore]> { let Latency = 10; // TODO: verify this
                                                     let ResourceCycles = [2]; }
def : WriteRes<WriteHMMA16816, [SM80UnitTensorCore]> { let Latency = 20; // At least 20
                                                       let ResourceCycles = [16]; }

// Memory
// shared memory (4 cycles for LDS.32 (w/o bank conflict))
def : WriteRes<WriteLDS, [SM80UnitLDST]> { let Latency = 8; // Note: this is an estimation
                                           // the ldst buffer can take care of some of this
                                           let ResourceCycles = [2] /*[4]*/; }
def : WriteRes<WriteSTS, [SM80UnitLDST]> { let Latency = 10; 
                                           let ResourceCycles = [2] /*[4]*/; }
def : WriteRes<WriteLDS64, [SM80UnitLDST]> { let Latency = 10; 
                                             let ResourceCycles = [6] /*[8]*/; }
def : WriteRes<WriteSTS64, [SM80UnitLDST]> { let Latency = 10; 
                                             let ResourceCycles = [6] /*[8]*/; }
def : WriteRes<WriteLDS128, [SM80UnitLDST]> { let Latency = 10; 
                                              let ResourceCycles = [20] /*[16]*/; }
def : WriteRes<WriteSTS128, [SM80UnitLDST]> { let Latency = 10; 
                                              let ResourceCycles = [20] /*[16]*/; }
// global memory
def : WriteRes<WriteLDG, [SM80UnitLDST]> { let Latency = 250; 
                                           // TODO: verify this.
                                           let ResourceCycles = [20] /*[40]*/; }
def : WriteRes<WriteSTG, [SM80UnitLDST]> { let Latency = 100; 
                                           let ResourceCycles = [16]; } // TODO: lat?

// Refine Basic Model
// 1. InstRW is used to refine scheduling info, override the target defaults
// Miscellaneous
def : InstRW<[WriteINT], (instrs COPY)>;
// 2. WriteSequence (?)
}