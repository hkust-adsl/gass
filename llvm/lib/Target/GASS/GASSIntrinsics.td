//=----------------------------------=//
// Load/Store
//=----------------------------------=//
// // async copy on Ampere
// multiclass LDGSTS<int Width> {
//   def r : GASSInstLdst<(outs), (ins),
//                        ,
//                        "LDGSTS", [], Width>;
// }

// let mayStore = true, mayLoad = true in {
// defm LDGSTS32 : LDGSTS<32>, Sched<[WriteLDG]>; // TODO: revisit this.
// defm LDGSTS64 : LDGSTS<64>, Sched<[WriteLDG]>;
// defm LDGSTS128 : LDGSTS<128>, Sced<[WriteLDG]>;
// }

// cp.async.commit_group;
// TODO: schedule model
def LDGDEPBAR : GASSInst<(outs), (ins), 0x9af, "LDGDEPBAR;", 
                         [(int_nvvm_cp_async_commit_group)]> {
  let hasSideEffects = true;
}

// // cp.async.wait_group N;
// // TODO: schedule model
// def DEPBAR : GASSInstOp2ri<(outs), (ins i32imm:$n), , "DEPBAR \t$;",
//     [(int_nvvm_cp_async_wait_group (i32 timm:$n))]>;

// // ldmatrix
// let mayLoad = true in {
// def LDSM : GASSInstLdst<(outs), (ins), 0x83b, "LDSM;", >;
// }

//=---------------------------------=//
// Tensor Core
//=---------------------------------=//
// multiclass HMMA1688 {
//   // Accumulator type
//   def _f32 : ;

//   def _f16 : ;
// }
// defm HMMA1688 : HMMA1688, Sched<[WriteTC]>;

// TODO: also to support _f16_f32 & _f16_f16
def HMMA884_f32_f32_Pseudo : 
  GASSPseudoInst<(outs VReg32:$d0, VReg32:$d1, VReg32:$d2, VReg32:$d3,
                       VReg32:$d4, VReg32:$d5, VReg32:$d6, VReg32:$d7),
                 (ins VReg32:$a0, VReg32:$a1, VReg32:$b0, VReg32:$b1,
                      VReg32:$c0, VReg32:$c1, VReg32:$c2, VReg32:$c3,
                      VReg32:$c4, VReg32:$c5, VReg32:$c6, VReg32:$c7,
                      MmaLayout:$s0layout, MmaLayout:$s1layout), []>, 
  Sched<[WriteHMMA884]>;

def : Pat<(int_nvvm_mma_m8n8k4_row_row_f32_f32 
             v2f16:$a0, v2f16:$a1, v2f16:$b0, v2f16:$b1,
             f32:$c0, f32:$c1, f32:$c2, f32:$c3, f32:$c4, f32:$c5, f32:$c6, f32:$c7), 
          (HMMA884_f32_f32_Pseudo 
             VReg32:$a0, VReg32:$a1, VReg32:$b0, VReg32:$b1,
             VReg32:$c0, VReg32:$c1, VReg32:$c2, VReg32:$c3, VReg32:$c4,
             VReg32:$c5, VReg32:$c6, VReg32:$c7, MmaRow, MmaRow)>;
def : Pat<(int_nvvm_mma_m8n8k4_row_col_f32_f32 
             v2f16:$a0, v2f16:$a1, v2f16:$b0, v2f16:$b1,
             f32:$c0, f32:$c1, f32:$c2, f32:$c3, f32:$c4, f32:$c5, f32:$c6, f32:$c7), 
          (HMMA884_f32_f32_Pseudo 
             VReg32:$a0, VReg32:$a1, VReg32:$b0, VReg32:$b1,
             VReg32:$c0, VReg32:$c1, VReg32:$c2, VReg32:$c3, VReg32:$c4,
             VReg32:$c5, VReg32:$c6, VReg32:$c7, MmaRow, MmaCol)>;
def : Pat<(int_nvvm_mma_m8n8k4_col_row_f32_f32 
             v2f16:$a0, v2f16:$a1, v2f16:$b0, v2f16:$b1,
             f32:$c0, f32:$c1, f32:$c2, f32:$c3, f32:$c4, f32:$c5, f32:$c6, f32:$c7), 
          (HMMA884_f32_f32_Pseudo 
             VReg32:$a0, VReg32:$a1, VReg32:$b0, VReg32:$b1,
             VReg32:$c0, VReg32:$c1, VReg32:$c2, VReg32:$c3, VReg32:$c4,
             VReg32:$c5, VReg32:$c6, VReg32:$c7, MmaCol, MmaRow)>;
def : Pat<(int_nvvm_mma_m8n8k4_col_col_f32_f32 
             v2f16:$a0, v2f16:$a1, v2f16:$b0, v2f16:$b1,
             f32:$c0, f32:$c1, f32:$c2, f32:$c3, f32:$c4, f32:$c5, f32:$c6, f32:$c7), 
          (HMMA884_f32_f32_Pseudo 
             VReg32:$a0, VReg32:$a1, VReg32:$b0, VReg32:$b1,
             VReg32:$c0, VReg32:$c1, VReg32:$c2, VReg32:$c3, VReg32:$c4,
             VReg32:$c5, VReg32:$c6, VReg32:$c7, MmaCol, MmaCol)>;

class HMMA884Base<dag oops, dag iops, bits<12> opcode, string asm> :
  GASSInstOp3rrr<oops, iops, opcode, asm, []> {
  bit s0layout;
  bit s1layout;
  bits<2> step;

  let Inst{9} = s0layout;
  let Inst{10} = s1layout;
  let Inst{16-15} = step;

  // TODO: revisit this
  // let isFixLat = false;
}
multiclass HMMA884 {
// dst, src0, src1, src2, src0layout, src1layout, step
  def _f32_f32 : 
    HMMA884Base<(outs VReg64:$dst), 
          (ins VReg64:$src0, VReg64:$src1, VReg64:$src2,
          MmaLayout:$s0layout, MmaLayout:$s1layout, MmaStep:$step), 0x236, 
          "HMMA.884.F32.F32$step \t$dst, $src0$s0layout, $src1$s1layout, $src2;"> {
      // .F32.F32
      let Inst{12} = 1;
      let Inst{14} = 1;

      // Always reuse registers
      let Inst{58} = 1;
      let Inst{59} = 1;
    }

  // TODO
  // def _f16_f16 : ;

  // TODO
  // def _f16_f32 : ;
}
defm HMMA884 : HMMA884, Sched<[WriteHMMA884]>;

// // mma16816
// multiclass HMMA16816 {
//   def _f16_f32 : ;

//   def _f16_f16 : ;
// }