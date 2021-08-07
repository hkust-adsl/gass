multiclass ULDC<RegisterClass RC, int Width> {
  def c : GASSPseudoUInst<(outs RC:$dst), (ins i32const:$offset), []>;
  // def rc : ;
}

defm ULDC32 : ULDC<SReg32, 32>, Sched<[WriteINT]>, Requires<[hasSM75]>;
defm ULDC64 : ULDC<SReg64, 64>, Sched<[WriteINT]>, Requires<[hasSM75]>;

// TODO:
//   UISETP, UIADD3, UMOV, USHF, UIMAD
// Other:
//   ULEA, ULOP3, 
// ISel: (VReg + UReg)
//   LDSM, 
//   STS,  (also for Turing?)
//   LDGSTS
class UREAD_SREG_R32<string RegName, bits<8> RegEnc>
  : GASSUInstOp0<(outs SReg32:$dst), (ins),
                0x9c3, "S2UR \t$dst, "#RegName#";", []>, Sched<[WriteLDS]> {
  let isFixLat = false;
  let Inst{15-8} = RegEnc;
}

def UREAD_CTAID_X : UREAD_SREG_R32<"ctaid.x", 37>;
def UREAD_CTAID_Y : UREAD_SREG_R32<"ctaid.y", 38>;
def UREAD_CTAID_Z : UREAD_SREG_R32<"ctaid.z", 39>;
def UREAD_CTAID_W : UREAD_SREG_R32<"ctaid.w", 40>;

// Pseudo
multiclass USHIFT {
  def 32rr : GASSPseudoUInst<(outs SReg32:$dst),
                            (ins SReg32:$src0, SReg32:$src1), []>;
  def 32ri : GASSPseudoUInst<(outs SReg32:$dst),
                            (ins SReg32:$src0, i32imm:$src1), []>;
}
defm USHL : USHIFT, Sched<[WriteSHIFT]>, Requires<[hasSM75]>; // TODO: Uniform
defm USRA : USHIFT, Sched<[WriteSHIFT]>, Requires<[hasSM75]>;
defm USRL : USHIFT, Sched<[WriteSHIFT]>, Requires<[hasSM75]>;

multiclass UBITWISE {
  def 32rr : GASSPseudoUInst<(outs SReg32:$dst), (ins SReg32:$src0, SReg32:$src1), []>;
  def 32ri : GASSPseudoUInst<(outs SReg32:$dst), (ins SReg32:$src0, i32imm:$src1), []>;
}
defm UOR  : UBITWISE, Sched<[WriteINT]>; // TODO: Uniform
defm UXOR : UBITWISE, Sched<[WriteINT]>;
defm UAND : UBITWISE, Sched<[WriteINT]>;

// USHIFT
multiclass USHF {
  def rrr : GASSUInstOp3rrr<(outs SReg32:$dst),
                       (ins SReg32:$src0, SReg32:$src1, SReg32:$src2,
                            ShiftDir:$dir, ShiftType:$ty, ShiftLoc:$hi),
                       0x299, // TODO: verify this
                       "USHF$dir$ty$hi \t$dst, $src0, $src1, $src2;",
                       []> {
    bits<1> dir; // .L | .R
    let Inst{12} = dir;
    bits<2> ty; // .S64 | .S32 | .U32
    let Inst{10-9} = ty;
    bits<1> hi; // .HI | .LO
    let Inst{16} = hi;
  }

  def rir : GASSUInstOp3rir<(outs SReg32:$dst),
                     (ins SReg32:$src0, i32imm:$src1, SReg32:$src2,
                          ShiftDir:$dir, ShiftType:$ty, ShiftLoc:$hi),
                     0x899,
                     "USHF$dir$ty$hi \t$dst, $src0, $src1, $src2;",
                     []> {
    bits<1> dir; // .L | .R
    let Inst{12} = dir;
    bits<2> ty; // .S64 | .S32 | .U32
    let Inst{10-9} = ty;
    bits<1> hi; // .HI | .LO
    let Inst{16} = hi;
  }
}
defm USHF : USHF, Sched<[WriteSHIFT]>;

multiclass UIADD {
  def rr : GASSUInstOp2rr<(outs SReg32:$dst), (ins SReg32:$src0, SReg32:$src1),
                         0x290, "UIADD3 \t$dst, $src0, $src1, URZ;", []> {
    let Src2 = 0x3f; // URZ
    let Inst{16-13} = 0xf; // no ps1
    let Inst{26-23} = 0xf; // no ps0
    let Inst{19-17} = 0x7; // no pd0
    let Inst{22-20} = 0x7; // no pd1
  }

  def ri : GASSUInstOp2rr<(outs SReg32:$dst), (ins SReg32:$src0, i32imm:$src1),
                         0x890, "UIADD3 \t$dst, $src0, $src1, URZ;", []> {
    let Src2 = 0x3f; // URZ
    let Inst{16-13} = 0xf; // no ps1
    let Inst{26-23} = 0xf; // no ps0
    let Inst{19-17} = 0x7; // no pd0
    let Inst{22-20} = 0x7; // no pd1
  }
}
defm UIADD : UIADD, Sched<[WriteINT]>; // TODO: Uniform

multiclass USUB {
  // TODO: Keep this as pseudo instr and add <negsrc1> to IADD3
  def rr : GASSUInstOp2rr<(outs SReg32:$dst), (ins SReg32:$src0, SReg32:$src1),
                          0x290, "UIADD3 $dst, $src0, -$src1, URZ;", []> {
    let Src2 = 0x3f;
    let Inst{16-13} = 0xf; // no ps1
    let Inst{26-23} = 0xf; // no ps0
    let Inst{19-17} = 0x7; // no pd0
    let Inst{22-20} = 0x7; // no pd1
    let Inst{127} = 1; // - negsrc1
  }
}
defm USUB : USUB, Sched<[WriteINT]>;

multiclass USETP<string OpcStr, RegisterClass RC, Operand ImmCls,
                bits<12> rrEnc, bits<12> riEnc> {
  def rr : GASSUInstOp2rr<(outs SReg1:$pdst), // TODO: 2 dsts (no?)
                      (ins RC:$src0, RC:$src1, CmpMode:$cmp, CmpModeSign:$sign),
                      rrEnc, !strconcat(OpcStr, "$cmp$sign \t$pdst, $src0, $src1;"),
                      []> {
    let dst = 0; // no $dst
    bits<3> pdst;
    bits<3> cmp;
    bit sign;
    
    let Inst{19-17} = pdst;
    let Inst{22-20} = 0x7; // pdst1
    let Inst{14-12} = cmp;
    let Inst{9} = sign;

    let Inst{25-23} = 0x7; // ps0 (PT)
    let Inst{6-4} = 0x7; // ?
  }

  def ri : GASSUInstOp2ri<(outs SReg1:$pdst),
                  (ins RC:$src0, ImmCls:$src1, CmpMode:$cmp, CmpModeSign:$sign),
                  riEnc, !strconcat(OpcStr, "$cmp$sign \t$pdst, $src0, $src1;"),
                  []> {
    let dst = 0;
    bits<3> pdst;
    bits<3> cmp;
    bit sign;
    
    let Inst{19-17} = pdst;
    let Inst{22-20} = 0x7; // pdst1
    let Inst{14-12} = cmp;
    let Inst{9} = sign;

    let Inst{25-23} = 0x7; // ps0 (PT)
    let Inst{6-4} = 0x7; // ?
  }
}
defm UISETP : USETP<"UISETP", SReg32, i32imm, 0x28c, 0x88c>, 
              Sched<[WriteISETP]>;

multiclass UMOV {
  def r : GASSUInstOp1r<(outs SReg32:$dst), (ins SReg32:$src0),
                       0xc82, "UMOV \t$dst, $src0;", []>;
  def i : GASSUInstOp1i<(outs SReg32:$dst), (ins i32imm:$src0),
                       0x882, "UMOV \t$dst, $src0;", []>;
}
defm UMOV32 : UMOV, Sched<[WriteINT]>;

class UCBRA : GASSPseudoUInst<(outs), (ins i32imm:$not, SReg1:$a, brtarget:$target), []>;
def UCBRA : UCBRA;