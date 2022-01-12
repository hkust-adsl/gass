//=----------------------------------=//
// Load/Store
//=----------------------------------=//
// async copy on Ampere
multiclass LDGSTS<int Width> {
  // Always riri?
  def riri : GASSInstLdstGlobalOff<(outs), 
      (ins VReg32:$dst, simm20:$doff, VReg64:$ptr, simm12:$soff, VReg1:$pred), // TOOD: pred mask
      0xfae, "LDGSTS.E."#Width#" \t[$dst+$doff], [$ptr.64+$soff];", [], Width> {
    // let Inst{7-0} = 0x46; // TODO: update this
    //    ? .64
    // let Inst{12} = 1; // TODO: why? (?)
    // let Inst{27} = 1; // always 1
    
    // let Inst{17} = // bypass -> 0, not-bypass -> 1
    // let Inst{18} = // not-zfill -> 0, zfill -> 1
    // let Inst{15-13} = 
    //    0->DEFAULT(nothing),  1->CONSTANT.PRIVATE, 2->CONSTANT.CTA, 3->CONSTANT.CTA.PRIVATE
    //    4->CONSTANT, 5->MMIO.CTA, 6->GPU.PRIVATE, 7->MMIO.CTA.PRIVATE
    // let Inst{22-20} = 
    //    0->.EF, 1->?, 2->.EL, 3->.LU, 4->.EU, 5->.NA(?)
    let Inst{27-0} = 0xb901d46;

    let Inst{15-13} = 4; // .CONSTANT
    bits<3> pred;
    let Inst{25-23} = pred; // pred mask
    bits<20> doff;
    bits<12> soff;
    let Inst{127-108} = doff;
    let Inst{107-96} = soff;
  }

  // def rir : ;

  // def rri : ;

  // def riri : ;

}

let mayStore = true, mayLoad = true in {
// defm LDGSTS32 : LDGSTS<32>, Sched<[WriteLDG]>; // TODO: revisit this.
// defm LDGSTS64 : LDGSTS<64>, Sched<[WriteLDG]>;
defm LDGSTS128 : LDGSTS<128>, Sched<[WriteLDGSTS]>, Requires<[hasSM80]>;
}

// ref: RISCVInstrInfo.td:1081
def : Pat<(int_nvvm_cp_async_cg_shared_global_16_p (add VReg32:$dbase, simm20:$off), VReg64:$ptr, VReg1:$pred), 
          (LDGSTS128riri VReg32:$dbase, simm20:$off, VReg64:$ptr, 0, VReg1:$pred)>;
def : Pat<(int_nvvm_cp_async_cg_shared_global_16_p VReg32:$dbase, VReg64:$ptr, VReg1:$pred),
          (LDGSTS128riri VReg32:$dbase, 0, VReg64:$ptr, 0, VReg1:$pred)>;

//----------shared memory----------------
class LDSMBase<dag outs, dag ins, string asmstr, int Num> 
  : GASSInst<outs, ins, 0x83b, asmstr, []> {
  let Inst{9-8} = !cond(
    !eq(Num, 1): 0,
    !eq(Num, 2): 1,
    !eq(Num, 4): 2
  );

  // Flag Encoding
  bits<1> trans;
  let Inst{14} = trans;

  // Operand Encoding
  bits<8> dst;
  bits<8> ptr;
  bits<24> offset;

  let Dst = dst;
  let Src0 = ptr;
  let Inst{127-104} = offset;
}

let isFixLat = false, mayLoad = true in
multiclass LDSM<RegisterClass RC, int Num> {
  def _ri : LDSMBase<(outs RC:$dst), (ins VReg32:$ptr, i32imm:$offset, LdsmTrans:$trans), 
                     "LDSM.16.$trans."#Num#"\t $dst, [$ptr+$offset];", Num>;
  def _rui : LDSMBase<(outs RC:$dst), (ins VReg32:$ptr, SReg32:$uoff, i32imm:$offset, LdsmTrans:$trans),
                      "LDSM.16.$trans."#Num#"\t $dst, [$ptr+$uoff+$offset];", Num> {
    bits<8> uoff;
    let Inst{103-96} = uoff;
    let Inst{27} = 1; // has UR offset;
  }

  def _ri_pseudo : GASSPseudoInstNoPred<(outs VReg32:$d0, VReg32:$d1, VReg32:$d2, VReg32:$d3), 
                                  (ins VReg32:$ptr, i32imm:$ioff, LdsmTrans:$trans), []>;
  def _rui_pseudo : GASSPseudoInstNoPred<(outs VReg32:$d0, VReg32:$d1, VReg32:$d2, VReg32:$d3), 
                                   (ins VReg32:$ptr, SReg32:$uoff, i32imm:$ioff, LdsmTrans:$trans), []>;
}

defm LDSM_x1 : LDSM<VReg32, 1>, Sched<[WriteLDS]>, Requires<[hasSM80]>;
defm LDSM_x2 : LDSM<VReg64, 2>, Sched<[WriteLDS64]>, Requires<[hasSM80]>;
defm LDSM_x4 : LDSM<VReg128, 4>, Sched<[WriteLDS128]>, Requires<[hasSM75]>;

// cp.async.commit_group;
// TODO: schedule model
def LDGDEPBAR : GASSInst<(outs), (ins), 0x9af, "LDGDEPBAR;", 
                         [(int_nvvm_cp_async_commit_group)]>, 
                         Sched<[WriteNOP]>, Requires<[hasSM80]> {
  let hasSideEffects = true;
}

// cp.async.wait_group N;
// TODO: barrier idx
def DEPBAR : GASSInst<(outs), (ins i32imm:$n), 0x91a, "DEPBAR.LE\t SB0, $n;",
    [(int_nvvm_cp_async_wait_group (i32 timm:$n))]>, Sched<[WriteNOP]>, Requires<[hasSM75]> {
  bits<6> n;
  bits<3> BarIdx = 0;

  let Inst{107-102} = n;  
  let Inst{110-108} = BarIdx;
  let Inst{111} = 1; // .LE

  let hasSideEffects = true;
}

//=---------------------------------=//
// MUFU
//=---------------------------------=//
class MUFU : GASSInst<(outs VReg32:$dst), (ins VReg32:$src1, MufuFlag:$flag), 
                     0x308, "MUFU.$flag \t$dst, $src1;", []> {
  bits<8> src1;
  let Src1 = src1;

  bits<4> flag;
  let Inst{13-10} = flag;
}

def MUFU : MUFU, Sched<[WriteMUFU]>;
def : Pat<(int_nvvm_cos_approx_f f32:$src), (MUFU VReg32:$src, MufuCos)>;
def : Pat<(int_nvvm_sin_approx_f f32:$src), (MUFU VReg32:$src, MufuSin)>;
def : Pat<(int_nvvm_ex2_approx_f f32:$src), (MUFU VReg32:$src, MufuEx2)>;
def : Pat<(int_nvvm_lg2_approx_f f32:$src), (MUFU VReg32:$src, MufuLg2)>;
def : Pat<(int_nvvm_rcp_rn_f     f32:$src), (MUFU VReg32:$src, MufuRcp)>;
// TODO: other flags...

def : Pat<(fdiv f32:$src0, f32:$src1), 
          (FMULrr VReg32:$src0, (MUFU VReg32:$src1, MufuRcp))>;

//=---------------------------------=//
// maxmin
//=---------------------------------=//
multiclass FMNMX<bit IsMin> {
  def rr : GASSInstOp2rr<(outs VReg32:$dst), (ins VReg32:$src0, VReg32:$src1),
                         0x209, 
                         !strconcat("FMNMX \t$dst, $src0, $src1, ",
                                    !if(IsMin, "", "!"), "PT;"), []> {
    bits<4> MNMXP = !if(IsMin, 0x7, 0xf);
    let Inst{26-23} = MNMXP;
  }
}

defm FMIN : FMNMX<true>, Sched<[WriteFMNMX]>;
defm FMAX : FMNMX<false>, Sched<[WriteFMNMX]>;

def : Pat<(fminnum f32:$src0, f32:$src1), (FMINrr VReg32:$src0, VReg32:$src1)>;
def : Pat<(fmaxnum f32:$src0, f32:$src1), (FMAXrr VReg32:$src0, VReg32:$src1)>;

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

// mma16816
// We need a pseudo instr because we need to insert REGSEQUENCEs pre-ra
def HMMA16816_f32_f16_f16_f32_Pseudo :
  GASSPseudoInstNoPred<(outs VReg32:$d0, VReg32:$d1, VReg32:$d2, VReg32:$d3),
                       (ins VReg32:$a0, VReg32:$a1, VReg32:$a2, VReg32:$a3,
                            VReg32:$b0, VReg32:$b1,
                            VReg32:$c0, VReg32:$c1, VReg32:$c2, VReg32:$c3), []>;
def : Pat<(int_nvvm_mma_m16n8k16_row_col_f32_f16_f16_f32
               v2f16:$a0, v2f16:$a1, v2f16:$a2, v2f16:$a3, v2f16:$b0, v2f16:$b1,
               f32:$c0, f32:$c1, f32:$c2, f32:$c3),
          (HMMA16816_f32_f16_f16_f32_Pseudo
               VReg32:$a0, VReg32:$a1, VReg32:$a2, VReg32:$a3, VReg32:$b0, VReg32:$b1,
               VReg32:$c0, VReg32:$c1, VReg32:$c2, VReg32:$c3)>;

multiclass HMMA16816 {
  def _f32_f16_f16_f32 
    : GASSInstOp3rrr<(outs VReg128:$dst),
               (ins VReg128:$src0, VReg64:$src1, VReg128:$src2),
               0x23c,
               "HMMA.16816.F32\t $dst, $src0, $src1, $src2;",
               []> {
    // .16816
    let Inst{11} = 1;
    // .F32
    let Inst{12} = 1;
    // Always reuse registers
    let Inst{58} = 1; // seems Turing doesn't need this
  }

  // TODO:
  // def _f32_f16_f16_f16 : ;
}

defm HMMA16816 : HMMA16816, Sched<[WriteHMMA16816]>;
