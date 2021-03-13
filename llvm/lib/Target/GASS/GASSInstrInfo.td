//=------------------------------------=//
// Format
//=------------------------------------=//
include "GASSInstrFormats.td"

// SDNode
def GASSexit : SDNode<"GASSISD::EXIT", SDTNone, 
    [SDNPHasChain, SDNPOptInGlue]>; // AMDGPU does this.

def GASSldc : SDNode<"GASSISD::LDC", SDTLoad, []>; // Constant load

//=---------------------------------=//
// Operands
//=---------------------------------=//
def constantmem : Operand<i32> {
  let PrintMethod = "printConstantMem";
  // let EncoderMethod = "encodeConstantMem"; // default: getMachineOpValue
}

def brtarget : Operand<OtherVT>;

def CmpMode : Operand<i32> {
  let PrintMethod = "printCmpMode";
  let EncoderMethod = "encodeCmpMode";
}

def CmpModeSign : Operand<i32> {
  let PrintMethod = "printCmpModeSign";
  let EncoderMethod = "encodeCmpModeSign";
}

// .xor .or .and
def BoolMode : Operand<i32> {
  let PrintMethod = "printBoolMode";
  let EncoderMethod = "encodeBoolMode";
}

// funnel shift flags
// SHF.{R|L}.{S32|U32}.{(HI)?}
def ShiftDir : Operand<i1> {
  let PrintMethod = "printShiftDir";
  let EncoderMethod = "encodeShiftDir";
}

def ShiftType : Operand<i8> {
  let PrintMethod = "printShiftType";
  let EncoderMethod = "encodeShiftType";
}

def ShiftLoc : Operand<i1> {
  let PrintMethod = "printShiftLoc";
  let EncoderMethod = "encodeShiftLoc";
}

// Shuffle (SHFL) flags
def ShflMode : Operand<i32> {
  let PrintMethod = "printShflMode";
}

// {!|}@p
def PredSign : Operand<i1> {
  let PrintMethod = "printPredicateSign";
}

// Tensor Cores
def MmaLayout : Operand<i32> {
  let PrintMethod = "printMmaLayout";
  // let EncoderMethod = "encodeMmaLayout";
}

def MmaStep : Operand<i32> {
  let PrintMethod = "printMmaStep";
  // let EncoderMethod = "encodeMmaStep";
}

// Must sync with enum in GASS.h
def MmaRow : PatLeaf<(i32 0)>;
def MmaCol : PatLeaf<(i32 1)>;

// Thest must match the enum in GASS.h
def CmpEQ   : PatLeaf<(i32 0)>;
def CmpNE   : PatLeaf<(i32 1)>;
def CmpLT   : PatLeaf<(i32 2)>;
def CmpLE   : PatLeaf<(i32 3)>;
def CmpGT   : PatLeaf<(i32 4)>;
def CmpGE   : PatLeaf<(i32 5)>;
def CmpEQU  : PatLeaf<(i32 10)>;
def CmpNEU  : PatLeaf<(i32 11)>;
def CmpLTU  : PatLeaf<(i32 12)>;
def CmpLEU  : PatLeaf<(i32 13)>;
def CmpGTU  : PatLeaf<(i32 14)>;
def CmpGEU  : PatLeaf<(i32 15)>;
def CmpNUM  : PatLeaf<(i32 16)>;
def CmpNAN  : PatLeaf<(i32 17)>;
// Sign of *SETP
def CmpU : PatLeaf<(i32 0)>;
def CmpS : PatLeaf<(i32 1)>;

//=---------------------------------=//
// Integer
//=---------------------------------=//
// IADD3, IMUL, IMAD, SHF, LOP3
multiclass IADD3 {
  def rrr : GASSInstOp3rrr<(outs VReg32:$dst),
                          (ins VReg32:$src0, VReg32:$src1, VReg32:$src2),
                          0x210,
                          "IADD3 \t$dst, $src0, $src1, $src2;",
                          [(set i32:$dst, (add (add i32:$src0, i32:$src1), 
                                                i32:$src2))]> {
    let Inst{16-13} = 0xf; // no ps1
    let Inst{26-23} = 0xf; // no ps0

    let Inst{19-17} = 0x7; // no pd0
    let Inst{22-20} = 0x7; // no pd1
  }

  def rir : GASSInstOp3rir<(outs VReg32:$dst),
                          (ins VReg32:$src0, i32imm:$src1, VReg32:$src2),
                          0x810,
                          "IADD3 \t$dst, $src0, $src1, $src2;",
                          [(set i32:$dst, (add (add i32:$src0, i32:$src1),
                                                imm:$src2))]> {
    let Inst{16-13} = 0xf; // no ps1
    let Inst{26-23} = 0xf; // no ps0

    let Inst{19-17} = 0x7; // no pd0
    let Inst{22-20} = 0x7; // no pd1
  }

  // Consider this
  // def _Xrrr : ;
}
defm IADD3 : IADD3, Sched<[WriteINT]>;
// Support 2-source add (TODO)
def : Pat<(i32 (add i32:$src0, i32:$src1)),
          (IADD3rir VReg32:$src0, PatLeaf<(i32 0)>, VReg32:$src1)>;

// 2-operand IADD, 
// This is IADD3 with default value
multiclass IADD {
  def rr : GASSInstOp2rr<(outs VReg32:$dst),
                    (ins VReg32:$src0, VReg32:$src1),
                    0x210,
                    "IADD3 \t$dst, $src0, $src1, RZ;",
                    [(set i32:$dst, (add i32:$src0, i32:$src1))]> {
    let Src2 = 0xff;

    let Inst{16-13} = 0xf; // no ps1
    let Inst{26-23} = 0xf; // no ps0

    let Inst{19-17} = 0x7; // no pd0
    let Inst{22-20} = 0x7; // no pd1
  }
  
  def ri : GASSInstOp2ri<(outs VReg32:$dst),
                    (ins VReg32:$src0, i32imm:$src1),
                    0x810,
                    "IADD3 \t$dst, $src0, $src1, RZ;",
                    [(set i32:$dst, (add i32:$src0, imm:$src1))]> {
    let Src2 = 0xff;

    let Inst{16-13} = 0xf; // no ps1
    let Inst{26-23} = 0xf; // no ps0

    let Inst{19-17} = 0x7; // no pd0
    let Inst{22-20} = 0x7; // no pd1
  }
}
defm IADD : IADD, Sched<[WriteINT]>;

// lhs/rhs ADDX (ADDCARRY)
// This is (real) IADD3 with defalut value
multiclass IADDX {
  def rr : GASSInstOp2rr<(outs VReg32:$dst, VReg1:$carryout),
                    (ins VReg32:$src0, VReg32:$src1, 
                        PredSign:$carryin_sign, VReg1:$carryin),
                    0x210,
                    "IADD3.X \t$dst, $carryout, $src0, $src1, RZ, $carryin_sign$carryin",
                    [/*TODO*/]> {
    let Inst{10} = 1; // .X
    let Src2 = 0xff;  // RZ

    // pd0
    bits<3> carryout;
    let Inst{19-17} = carryout;
    // pd1
    let Inst{22-20} = 0x7; 

    // ps0
    bit carryin_sign;
    bits<3> carryin;
    let Inst{26} = carryin_sign;
    let Inst{25-23} = carryin;
    // ps1
    let Inst{16-13} = 0xf;
  }

  def ri : GASSInstOp2ri<(outs VReg32:$dst, VReg1:$carryout),
                    (ins VReg32:$src0, i32imm:$src1, 
                         PredSign:$carryin_sign, VReg1:$carryin),
                    0x810,
                    "IADD3.X \t$dst, $carryout, $src0, $src1, RZ, $carryin_sign$carryin",
                    [/*TODO*/]> {
    let Inst{10} = 1; // .X
    let Src2 = 0xff; // RZ

    // pd0
    bits<3> carryout;
    let Inst{19-17} = carryout;
    // pd1
    let Inst{22-20} = 0x7; 

    // ps0
    bit carryin_sign;
    bits<3> carryin;
    let Inst{26} = carryin_sign;
    let Inst{25-23} = carryin;
    // ps1
    let Inst{16-13} = 0xf;
  }
}
defm IADDX : IADDX, Sched<[WriteINT]>;

// i64 add (pseudo instruction)
multiclass IADD64 {
  def rr : GASSInst<(outs VReg64:$dst),
                     (ins VReg64:$src0, VReg64:$src1),
                     0x0, "",
                     [(set i64:$dst, (add i64:$src0, i64:$src1))]> {
    let isPseudo = true;
  }

  // def ri : ;
}
defm IADD64 : IADD64, Sched<[WriteINT]>;

multiclass SUB {
  // TODO: Keep this as pseudo instr and add <negsrc1> to IADD3
  def rr : GASSInstOp2rr<(outs VReg32:$dst),
                          (ins VReg32:$src0, VReg32:$src1),
                          0x210, "IADD3 $dst, $src0, -$src1, RZ;",
                          [(set i32:$dst, (sub i32:$src0, i32:$src1))]> {
    let Src2 = 0xff;

    let Inst{16-13} = 0xf; // no ps1
    let Inst{26-23} = 0xf; // no ps0

    let Inst{19-17} = 0x7; // no pd0
    let Inst{22-20} = 0x7; // no pd1

    let Inst{127} = 1; // - negsrc1
  }

  def ri : GASSPseudoInst<(outs VReg32:$dst),
                          (ins VReg32:$src0, i32imm:$src1),
                          [(set i32:$dst, (sub i32:$src0, imm:$src1))]>;
}

defm SUB : SUB, Sched<[WriteINT]>;

// IMUL
// Pseudo instruction
multiclass IMUL {
  def rr : GASSPseudoInst<(outs VReg32:$dst),
                    (ins VReg32:$src0, VReg32:$src1),
                    [(set i32:$dst, (mul i32:$src0, i32:$src1))]>;

  def _WIDErr : GASSPseudoInst<(outs VReg64:$dst),
                         (ins VReg32:$src0, VReg32:$src1),
                         []>;

  def _WIDEri : GASSPseudoInst<(outs VReg64:$dst),
                         (ins VReg32:$src0, i32imm:$src1),
                         []>;
}
defm IMUL : IMUL, Sched<[WriteINT]>;

// IMAD
multiclass IMAD<bit IsSigned> {
  def rrr : GASSInstOp3rrr<(outs VReg32:$dst),
                     (ins VReg32:$src0, VReg32:$src1, VReg32:$src2),
                     0x224,
                     "IMAD \t$dst, $src0, $src1, $src2;",
                     [(set i32:$dst, (add (mul i32:$src0, i32:$src1), 
                                          i32:$src2))]>;

  def rri : GASSInstOp3rri<(outs VReg32:$dst),
                     (ins VReg32:$src0, VReg32:$src1, i32imm:$src2),
                     0x424,
                     "IMAD \t$dst, $src0, $src1, $src2;",
                     [(set i32:$dst, (add (mul i32:$src0, i32:$src1),
                                          imm:$src2))]>;

  def rir : GASSInstOp3rir<(outs VReg32:$dst),
                     (ins VReg32:$src0, i32imm:$src1, VReg32:$src2),
                     0x824,
                     "IMAD \t$dst, $src0, $src1, $src2;",
                     [(set i32:$dst, (add (mul i32:$src0, imm:$src1),
                                          i32:$src2))]>;

  def _WIDErrr : GASSInstOp3rrr<(outs VReg64:$dst),
                          (ins VReg32:$src0, VReg32:$src1, VReg64:$src2),
                          0x225,
                          "IMAD.WIDE \t$dst, $src0, $src1, $src2;",
                          // Does this work?
                          []>;

  // def _WIDErri : ;

  def _WIDErir : GASSInstOp3rir<(outs VReg64:$dst),
                          (ins VReg32:$src0, i32imm:$src1, VReg64:$src2),
                          0x825,
                          "IMAD.WIDE \t$dst, $src0, $src1, $src2;",
                          []>;

  // TODO: fill this.
  // def _HIrrr : ;
}
defm IMAD_S32 : IMAD<true>, Sched<[WriteFP32]>;
// defm IMAD_U32 : IMAD<false>, Sched<[WriteFP32]>;

// SHF
// ins:
//   Lo32bits, shiftAmount, Hi32Bits, l/r, type, lo/hi
// TODO: we need a base for shf for encoding
multiclass SHF {
  def rrr : GASSInstOp3rrr<(outs VReg32:$dst),
                       (ins VReg32:$src0, VReg32:$src1, VReg32:$src2,
                            ShiftDir:$dir, ShiftType:$ty, ShiftLoc:$hi),
                       0x219,
                       "SHF$dir$ty$hi \t$dst, $src0, $src1, $src2;",
                       []> {
    bits<1> dir; // .L | .R
    let Inst{12} = dir;
    bits<2> ty; // .S64 | .S32 | .U32
    let Inst{10-9} = ty;
    bits<1> hi; // .HI | .LO
    let Inst{16} = hi;
  }

  def rir : GASSInstOp3rir<(outs VReg32:$dst),
                     (ins VReg32:$src0, VReg32:$src1, i32imm:$src2,
                          ShiftDir:$dir, ShiftType:$ty, ShiftLoc:$hi),
                     0x819,
                     "SHF$dir$ty$hi \t$dst, $src0, $src1, $src2;",
                     []> {
    bits<1> dir; // .L | .R
    let Inst{12} = dir;
    bits<2> ty; // .S64 | .S32 | .U32
    let Inst{10-9} = ty;
    bits<1> hi; // .HI | .LO
    let Inst{16} = hi;
  }
}
defm SHF : SHF, Sched<[WriteINT]>;

// LOP3
//   LOP3 $dst, $src1, $src2, $src3, $immLut;
multiclass LOP3 {
  def rrr : GASSInstOp3rrr<(outs VReg32:$dst),
                     (ins VReg32:$src0, VReg32:$src1, VReg32:$src2, 
                          i8imm:$immLut),
                     0x212,
                     "LOP3.LUT $dst, $src0, $src1, $src2, $immLut;",
                     []>;

  def rri : GASSInstOp3rri<(outs VReg32:$dst),
                     (ins VReg32:$src0, VReg32:$src1, i32imm:$src2,
                          i8imm:$immLut),
                     0x412,
                     "LOP3.LUT $dst, $src0, $src1, $src2, $immLut;",
                     []>;

  def rir : GASSInstOp3rir<(outs VReg32:$dst),
                     (ins VReg32:$src0, i32imm:$src1, VReg32:$src2,
                          i8imm:$immLut),
                     0x812,
                     "LOP3.LUT $dst, $src0, $src1, $src2, $immLut;",
                     []>;

  // def rrc : ;
}
defm LOP3 : LOP3, Sched<[WriteINT]>;

multiclass SELECT<string OpcStr, ValueType VT, Operand ImmCls, ValueType ImmVT,
                  bits<12> rrEnc, bits<12> riEnc> {
  def rr : GASSInst<(outs VReg32:$dst),
                    (ins VReg32:$src0, VReg32:$src1, VReg1:$p),
                    rrEnc,
                    !strconcat(OpcStr, " \t$dst, $src0, $src1, $p;"),
                    [(set VT:$dst, (select i1:$p, VT:$src0, VT:$src1))]>;

  def ri : GASSInst<(outs VReg32:$dst),
                    (ins VReg32:$src0, ImmCls:$src1, VReg1:$p),
                    riEnc,
                    !strconcat(OpcStr, " \t$dst, $src0, $src1, $p;"),
                    [(set VT:$dst, (select i1:$p, VT:$src0, ImmVT:$src1))]>;

  // def rc : ;
}

// TODO: confirm riEnc
defm SEL  : SELECT<"SEL", i32, i32imm, imm, 0x207, 0x807>, Sched<[WriteINT]>;
defm FSEL : SELECT<"FSEL", f32, f32imm, fpimm, 0x208, 0x808>, Sched<[WriteINT]>;

// TODO: combine xor/or/and/not
//==------------------------------------------------------------------------==//
// xor/or/and/not (all pseudo)
//==------------------------------------------------------------------------==//
multiclass BITWISE<SDNode OpNode> {
  def 1rr : GASSPseudoInst<(outs VReg1:$dst),
                          (ins VReg1:$src0, VReg1:$src1),
                          [(set i1:$dst, (OpNode i1:$src0, i1:$src1))]>,
           Sched<[WriteINT]>;

  def 1ri : GASSPseudoInst<(outs VReg1:$dst),
                          (ins VReg1:$src0, VReg1:$src1),
                          [(set i1:$dst, (OpNode i1:$src0, imm:$src1))]>,
           Sched<[WriteINT]>;

  def 32rr : GASSPseudoInst<(outs VReg32:$dst),
                          (ins VReg32:$src0, VReg32:$src1),
                          [(set i32:$dst, (OpNode i32:$src0, i32:$src1))]>,
           Sched<[WriteINT]>;

  def 32ri : GASSPseudoInst<(outs VReg32:$dst),
                          (ins VReg32:$src0, VReg32:$src1),
                          [(set i32:$dst, (OpNode i32:$src0, imm:$src1))]>,
           Sched<[WriteINT]>;
}

defm OR  : BITWISE<or>;
defm XOR : BITWISE<xor>;
defm AND : BITWISE<and>;


//==------------------------------------------------------------------------==//
// Pseudo instruction
//==------------------------------------------------------------------------==//
// SHL/SHR are shf
// The shift amount is always 32 bits
multiclass SHIFT<SDNode OpNode> {
  def 64rr : GASSPseudoInst<(outs VReg64:$dst),
                            (ins VReg64:$src0, VReg32:$src1),
                            [(set i64:$dst, (OpNode i64:$src0, i32:$src1))]>;
  def 64ri : GASSPseudoInst<(outs VReg64:$dst),
                            (ins VReg64:$src0, i32imm:$src1),
                            [(set i64:$dst, (OpNode i64:$src0, 
                                                    (i32 imm:$src1)))]>;
  def 32rr : GASSPseudoInst<(outs VReg32:$dst),
                            (ins VReg32:$src0, VReg32:$src1),
                            [(set i32:$dst, (OpNode i32:$src0, i32:$src1))]>;
  def 32ri : GASSPseudoInst<(outs VReg32:$dst),
                            (ins VReg32:$src0, i32imm:$src1),
                            [(set i32:$dst, (OpNode i32:$src0,
                                                    (i32 imm:$src1)))]>;
}
defm SHL : SHIFT<shl>, Sched<[WriteINT]>;
defm SRA : SHIFT<sra>, Sched<[WriteINT]>;
defm SRL : SHIFT<srl>, Sched<[WriteINT]>;

// SIGN_EXTEND
// i32->i64
class SEXT : GASSPseudoInst<(outs VReg64:$dst),
                            (ins VReg32:$src0),
                            [(set i64:$dst, (sext i32:$src0))]>;
def SEXT : SEXT, Sched<[WriteINT]>;

class ZEXT : GASSPseudoInst<(outs VReg64:$dst),
                            (ins VReg32:$src0),
                            [(set i64:$dst, (zext i32:$src0))]>;
def ZEXT : ZEXT, Sched<[WriteINT]>;

//=-------------------------------------------------------------------------==//
// convert "sign/zero-extend, then shift left by an imm" to mul.wide
//=-------------------------------------------------------------------------==//
def SHL2MUL32 : SDNodeXForm<imm, [{
  const APInt &v = N->getAPIntValue();
  APInt temp(32, 1);
  return CurDAG->getTargetConstant(temp.shl(v), SDLoc(N), MVT::i32);
}]>;

def : Pat<(shl (sext i32:$a), (i64 imm:$b)),
               (IMUL_WIDEri VReg32:$a, (SHL2MUL32 node:$b))>;

// PRMT
multiclass PRMT {
  def rrr : GASSInstOp3rrr<(outs VReg32:$dst),
                           (ins VReg32:$src0, VReg32:$src1, VReg32:$src2),
                           0x216, "PRMT \t$dst, $src0, $src1, $src2;", []>;
  
  def rir : GASSInstOp3rir<(outs VReg32:$dst),
                           (ins VReg32:$src0, i32imm:$src1, VReg32:$src2),
                           0x816, "PRMT \t$dst, $src0, $src1, $src2;", []>;
}
defm PRMT : PRMT, Sched<[WriteINT]>;

//=---------------------------------=//
// float-point
//=---------------------------------=//
// order:
// for (type : {f32, f64, f16x2})
//   for (opcode : {fma, add, mul, setp})
// MUFU
// icr
// 0 0 0 0
//     1   rrr 0x2
//   1     rri 0x4
//   1 1   rrc 0x6
// 1       rir 0x8
// 1   1   rcr 0xa
multiclass FMA<string OpcStr, RegisterClass RC, Operand ImmCls, ValueType vt,
               bits<12> rrrEnc, bits<12> rriEnc, bits<12> rirEnc> {
  def rrr : GASSInstOp3rrr<(outs RC:$dst),
                    (ins RC:$a, RC:$b, RC:$c),
                    rrrEnc, 
                    !strconcat(OpcStr, " \t$dst, $a, $b, $c;"), 
                    [(set vt:$dst, (fma vt:$a, vt:$b, vt:$c))]>;
  
  def rri : GASSInstOp3rri<(outs RC:$dst),
                     (ins RC:$a, RC:$b, ImmCls:$c),
                     rriEnc,
                     !strconcat(OpcStr, " \t$dst, $a, $b, $c;"),
                     [(set vt:$dst, (fma vt:$a, vt:$b, fpimm:$c))]>;

  def rir : GASSInstOp3rir<(outs RC:$dst),
                     (ins RC:$a, ImmCls:$b, RC:$c),
                     rirEnc,
                     !strconcat(OpcStr, " \t$dst, $a, $b, $c;"),
                     [(set vt:$dst, (fma vt:$a, fpimm:$b, vt:$c))]>;

  // def rrc :
  // def rcr :
}

// MUL
multiclass MUL<string OpcStr, RegisterClass RC, Operand ImmCls, ValueType vt,
               bits<12> rrEnc, bits<12> riEnc> {
  def rr : GASSInst<(outs RC:$dst), (ins RC:$a, RC:$b),
                    rrEnc,
                    !strconcat(OpcStr, " \t$dst, $a, $b;"),
                    [(set vt:$dst, (fmul vt:$a, vt:$b))]>;
  
  def ri : GASSInst<(outs RC:$dst), (ins RC:$a, ImmCls:$b),
                     riEnc,
                     !strconcat(OpcStr, " \t$dst, $a, $b;"),
                     [(set vt:$dst, (fmul vt:$a, fpimm:$b))]>;
}

// ADD
multiclass ADD<string OpcStr, RegisterClass RC, Operand ImmCls, ValueType vt,
               bits<12> rrEnc, bits<12> riEnc> {
  def rr : GASSInstOp2rr<(outs RC:$dst), (ins RC:$src0, RC:$src1),
                    rrEnc,
                    !strconcat(OpcStr, " \t$dst, $src0, $src1;"),
                    [(set vt:$dst, (fadd vt:$src0, vt:$src1))]>;
  
  def ri : GASSInstOp2ri<(outs RC:$dst), (ins RC:$src0, ImmCls:$src1),
                     riEnc,
                     !strconcat(OpcStr, " \t$dst, $src0, $src1;"),
                     [(set vt:$dst, (fadd vt:$src0, fpimm:$src1))]>;
}

defm FFMA : FMA<"FFMA", VReg32, f32imm, f32, 0x223, 0x423, 0x823>, 
            Sched<[WriteFP32]>;
defm FMUL : MUL<"FMUL", VReg32, f32imm, f32, 0x220, 0x820>,
            Sched<[WriteFP32]>;
defm FADD : ADD<"FADD", VReg32, f32imm, f32, 0x221, 0x421>,
            Sched<[WriteFP32]>;

defm DFMA : FMA<"DFMA", VReg64, f64imm, f64, 0x22b, 0x42b, 0x82b> , 
            Sched<[WriteFP64]>;
defm DMUL : MUL<"DMUL", VReg64, f64imm, f64, 0x228, 0x828> , 
            Sched<[WriteFP64]>;
defm DADD : ADD<"DADD", VReg64, f64imm, f64, 0x229, 0x429> , 
            Sched<[WriteFP64]>;

// Patterns for fma
def : Pat<(f32 (fadd (fmul f32:$a, f32:$b), f32:$c)),
          (FFMArrr VReg32:$a, VReg32:$b, VReg32:$c)>;
def : Pat<(f32 (fadd (fmul f32:$a, f32:$b), fpimm:$c)),
          (FFMArri VReg32:$a, VReg32:$b, fpimm:$c)>;
def : Pat<(f32 (fadd (fmul f32:$a, fpimm:$b), f32:$c)),
          (FFMArir VReg32:$a, fpimm:$b, VReg32:$c)>;
def : Pat<(f64 (fadd (fmul f64:$a, f64:$b), f64:$c)),
          (DFMArrr VReg64:$a, VReg64:$b, VReg64:$c)>;
def : Pat<(f64 (fadd (fmul f64:$a, f64:$b), fpimm:$c)),
          (DFMArri VReg64:$a, VReg64:$b, fpimm:$c)>;
def : Pat<(f64 (fadd (fmul f64:$a, fpimm:$b), f64:$c)),
          (DFMArir VReg64:$a, fpimm:$b, VReg64:$c)>;

// half prevision
multiclass HFMA2<RegisterClass RC, bits<12> rrrEnc> {
  def rr : GASSInst<(outs RC:$dst), (ins RC:$a, RC:$b, RC:$c),
                    rrrEnc, "HFMA2 \t$dst, $a, $b, $c;",
                    [/*TODO*/]>;
}

multiclass HADD2<RegisterClass RC, bits<12> rrrEnc> {
  def rr : GASSInst<(outs RC:$dst),(ins RC:$a, RC:$b),
                    rrrEnc, "HADD2 \t$dst, $a, $b;",
                    [/*TODO*/]>;
}

multiclass HMUL2<RegisterClass RC, bits<12> rrrEnc> {
  def rr : GASSInst<(outs RC:$dst), (ins RC:$a, RC:$b), 
                     rrrEnc, "HMUL2 \t$dst, $a, $b;",
                     [/*TODO*/]>;
}

defm HFMA2 : HFMA2<VReg32, 0x231>, Sched<[WriteFP32]>; // Sched<[WriteFP16x2]?
defm HADD2 : HADD2<VReg32, 0x230>, Sched<[WriteFP32]>;
defm HMUL2 : HMUL2<VReg32, 0x232>, Sched<[WriteFP32]>;


//=---------------------------------=//
// setp
//=---------------------------------=//
// ISETP, FSETP, PSETP
// eg, ISETP.LT.AND.S32 P0, PT, $src0, $src1, $srcp;
multiclass SETP<string OpcStr, RegisterClass RC, Operand ImmCls,
                bits<12> rrEnc, bits<12> riEnc> {
  def rr : GASSInstOp2rr<(outs VReg1:$pdst), // TODO: 2 dsts (no?)
                      (ins RC:$src0, RC:$src1, CmpMode:$cmp, CmpModeSign:$sign),
                      rrEnc,
                      !strconcat(OpcStr, "$cmp$sign \t$pdst, $src0, $src1;"),
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

  def ri : GASSInstOp2ri<(outs VReg1:$pdst),
                  (ins RC:$src0, ImmCls:$src1, CmpMode:$cmp, CmpModeSign:$sign),
                  riEnc,
                  !strconcat(OpcStr, "$cmp$sign \t$pdst, $src0, $src1;"),
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
defm ISETP : SETP<"ISETP", VReg32, i32imm, 0x20c, 0x80c>, Sched<[WriteISETP]>;

// Patterns
// unsigned compare
def : Pat<(i1 (setueq i32:$a, i32:$b)), (ISETPrr VReg32:$a, VReg32:$b, CmpEQ, CmpU)>;
def : Pat<(i1 (setueq i32:$a, imm:$b)), (ISETPri VReg32:$a, imm:$b, CmpEQ, CmpU)>;
def : Pat<(i1 (setune i32:$a, i32:$b)), (ISETPrr VReg32:$a, VReg32:$b, CmpNE, CmpU)>;
def : Pat<(i1 (setune i32:$a, imm:$b)), (ISETPri VReg32:$a, imm:$b, CmpNE, CmpU)>;
def : Pat<(i1 (setult i32:$a, i32:$b)), (ISETPrr VReg32:$a, VReg32:$b, CmpLT, CmpU)>;
def : Pat<(i1 (setult i32:$a, imm:$b)), (ISETPri VReg32:$a, imm:$b, CmpLT, CmpU)>;
def : Pat<(i1 (setule i32:$a, i32:$b)), (ISETPrr VReg32:$a, VReg32:$b, CmpLE, CmpU)>;
def : Pat<(i1 (setule i32:$a, imm:$b)), (ISETPri VReg32:$a, imm:$b, CmpLE, CmpU)>;
def : Pat<(i1 (setugt i32:$a, i32:$b)), (ISETPrr VReg32:$a, VReg32:$b, CmpGT, CmpU)>;
def : Pat<(i1 (setugt i32:$a, imm:$b)), (ISETPri VReg32:$a, imm:$b, CmpGT, CmpU)>;
def : Pat<(i1 (setuge i32:$a, i32:$b)), (ISETPrr VReg32:$a, VReg32:$b, CmpGE, CmpU)>;
def : Pat<(i1 (setuge i32:$a, imm:$b)), (ISETPri VReg32:$a, imm:$b, CmpGE, CmpU)>;
// signed compare
def : Pat<(i1 (seteq i32:$a, i32:$b)), (ISETPrr VReg32:$a, VReg32:$b, CmpEQ, CmpS)>;
def : Pat<(i1 (seteq i32:$a, imm:$b)), (ISETPri VReg32:$a, imm:$b, CmpEQ, CmpS)>;
def : Pat<(i1 (setne i32:$a, i32:$b)), (ISETPrr VReg32:$a, VReg32:$b, CmpNE, CmpS)>;
def : Pat<(i1 (setne i32:$a, imm:$b)), (ISETPri VReg32:$a, imm:$b, CmpNE, CmpS)>;
def : Pat<(i1 (setlt i32:$a, i32:$b)), (ISETPrr VReg32:$a, VReg32:$b, CmpLT, CmpS)>;
def : Pat<(i1 (setlt i32:$a, imm:$b)), (ISETPri VReg32:$a, imm:$b, CmpLT, CmpS)>;
def : Pat<(i1 (setle i32:$a, i32:$b)), (ISETPrr VReg32:$a, VReg32:$b, CmpLE, CmpS)>;
def : Pat<(i1 (setle i32:$a, imm:$b)), (ISETPri VReg32:$a, imm:$b, CmpLE, CmpS)>;
def : Pat<(i1 (setgt i32:$a, i32:$b)), (ISETPrr VReg32:$a, VReg32:$b, CmpGT, CmpS)>;
def : Pat<(i1 (setgt i32:$a, imm:$b)), (ISETPri VReg32:$a, imm:$b, CmpGT, CmpS)>;
def : Pat<(i1 (setge i32:$a, i32:$b)), (ISETPrr VReg32:$a, VReg32:$b, CmpGE, CmpS)>;
def : Pat<(i1 (setge i32:$a, imm:$b)), (ISETPri VReg32:$a, imm:$b, CmpGE, CmpS)>;


// i1 compares
def : Pat<(setne i1:$a, i1:$b), (XOR1rr VReg1:$a, VReg1:$b)>;
def : Pat<(setne i1:$a, imm:$b), (XOR1ri VReg1:$a, imm:$b)>;
// def Pat<(set)


//=---------------------------------=//
// Conversion
//=---------------------------------=//
// F2F, F2I, I2F, I2I


//=---------------------------------=//
// Predicate
//=---------------------------------=//
// PLOP3, P2R, R2P


//=---------------------------------=//
// 
//=---------------------------------=//


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
  Sched<[WriteTC]>;

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
    }

  // TODO
  // def _f16_f16 : ;

  // TODO
  // def _f16_f32 : ;
}
defm HMMA884 : HMMA884, Sched<[WriteTC]>;


//=---------------------------------=//
// mov
//=---------------------------------=//
multiclass MOV<RegisterClass RC, int Width, Operand ImmCls, Operand FpImmCls> {
  def r : GASSInstOp1r<(outs RC:$dst),
                   (ins RC:$src0),
                   0x202, "MOV."#Width#" \t$dst, $src0;", []> {
    let Inst{11-8} = 0xf;
  }

  def i : GASSInstOp1i<(outs RC:$dst),
                   (ins ImmCls:$src0),
                   0x802, "MOV."#Width#" \t$dst, $src0;", []> {
    let Inst{11-8} = 0xf;
  }
  
  def f : GASSInstOp1i<(outs RC:$dst),
                   (ins FpImmCls:$src0),
                   0x802, "MOV."#Width#" \t$dst, $src0;", []> {
    let Inst{11-8} = 0xf;
  }

  def c : GASSInstOp1c<(outs RC:$dst),
                   (ins constantmem:$src0), // Not true.
                   0xa02, "MOV."#Width#" \t$dst, $src0;", []> {
    let Inst{11-8} = 0xf;
  }
}

defm MOV32 : MOV<VReg32, 32, i32imm, f32imm>, Sched<[WriteINT]>;
let isPseudo = true in {
defm MOV64 : MOV<VReg64, 64, i64imm, f64imm>, Sched<[WriteINT]>;
}


// TODO: this can't be right?
def : Pat<(i32 (imm:$src0)),   (MOV32i imm:$src0)>;
def : Pat<(f32 (fpimm:$src0)), (MOV32f fpimm:$src0)>;
def : Pat<(i64 (imm:$src0)),   (MOV64i imm:$src0)>;

//=---------------------------------=//
// load/store instructions
//=---------------------------------=//
// LDC, LDG, STG
// TODO: LD, LDS, LDSM, LDL, ST, STS
multiclass LDC<RegisterClass RC, int Width> {
  def c : GASSInstLdst<(outs RC:$dst),
                       (ins constantmem:$offset), 
                       0xb82,
                       "LDC." #Width #" \t$dst, $offset;", [], Width> {
    let ptr = 0xff; // base offset
    bits<32> offset; // TODO: check width of constant mem
    let Inst{127-102} = offset{25-0}; 
    let Inst{103-96} = 0; // Src1 
  }

  // def rc : ;
}

defm LDC32 : LDC<VReg32, 32>, Sched<[WriteLD]>;
defm LDC64 : LDC<VReg64, 64>, Sched<[WriteLD]>;

let mayLoad = true in {
multiclass LDG<RegisterClass RC, int Width> {
  def r : GASSInstLdstGlobal<(outs RC:$dst),
                       (ins VReg64:$ptr),
                       0x381,
                       "LDG."#Width#" \t$dst, [$ptr];", [], Width> {
    let Inst{19-17} = 0x7; // predicate: PT 
    // TODO: this is just a quick fix. But we should modify GASSInstLdst
    let Inst{103-96} = 0; // Src1 
  }

  def ri : GASSInstLdstGlobalOff<(outs RC:$dst),
                    (ins VReg64:$ptr, i16imm:$offset),
                    0x381,
                    "LDG."#Width#" \t$dst, [$ptr+$offset];", [], Width> {
    let Inst{19-17} = 0x7; // predicate: PT
    let Inst{103-96} = 0; // Src1 
  }
}
}

defm LDG32 : LDG<VReg32, 32>, Sched<[WriteLD]>;
defm LDG64 : LDG<VReg64, 64>, Sched<[WriteLD]>;
defm LDG128 : LDG<VReg128, 128>, Sched<[WriteLD]>;

// LDS
let mayLoad = true in {
multiclass LDS<RegisterClass RC, int Width> {
  def r : GASSInstLdst<(outs RC:$dst), (ins VReg32:$ptr),
                       0x984, "LDS."#Width#" \t$dst, [$ptr];", [],
                       Width>;

  def ri: GASSInstLdstOff<(outs RC:$dst), (ins VReg32:$ptr, i16imm:$offset),
                       0x984, "LDS."#Width#" \t$dst, [$ptr+$offset];", [],
                       Width>;
}
}
defm LDS16 : LDS<VReg32, 16>, Sched<[WriteLD]>;
defm LDS32 : LDS<VReg32, 32>, Sched<[WriteLD]>;
defm LDS64 : LDS<VReg64, 64>, Sched<[WriteLD]>;
defm LDS128 : LDS<VReg128, 128>, Sched<[WriteLD]>;


// STG
multiclass STG<RegisterClass RC, int Width> {
  def r : GASSInstLdstGlobal<(outs),
                       (ins RC:$src1, VReg64:$ptr), // RZ?
                       0x386,
                       "STG."#Width#" \t[$ptr], $src1;", [], Width> {
    let Inst{87-80} = 0; // Dst
  }
  
  def ri : GASSInstLdstGlobalOff<(outs),
                        (ins RC:$src1, VReg64:$ptr, i16imm:$offset),
                        0x386,
                        "STG."#Width#" \t[$ptr+$offset], $src1;", [], Width> {
    let Inst{87-80} = 0; // Dst
  }
}

defm STG32 : STG<VReg32, 32>, Sched<[WriteST]>;
defm STG64 : STG<VReg64, 64>, Sched<[WriteST]>;
defm STG128 : STG<VReg128, 128>, Sched<[WriteST]>;

multiclass STS<RegisterClass RC, int Width> {
  def r : GASSInstLdst<(outs), (ins RC:$src, VReg32:$ptr),
                       0x388,
                       "STS."#Width#" \t[$ptr], $src;", [], Width>;
  
  def ri : GASSInstLdstOff<(outs), (ins RC:$src, VReg32:$ptr, i16imm:$offset),
                        0x388,
                        "STS."#Width#" \t[$ptr+$offset], $src;", [], Width>;
}
defm STS16 : STS<VReg32, 16>, Sched<[WriteST]>;
defm STS32 : STS<VReg32, 32>, Sched<[WriteST]>;
defm STS64 : STS<VReg64, 64>, Sched<[WriteST]>;
defm STS128 : STS<VReg128, 128>, Sched<[WriteST]>;

//=---------------------------------=//
// Control Flow
//=---------------------------------=//
// BRA, CBRA (condition branch), BAR,
// TODO: BSSY, BSYNC, WARPSYNC (?)
let isTerminator = true, isBranch = true in {
  let isBarrier = true in 
    def BRA : GASSInstNoPred<(outs), 
                             (ins brtarget:$target), 
                             0x947, 
                             "BRA \t$target;", 
                             [(br bb:$target)]>, Sched<[]> {
      let Inst{26-23} = 0x7; // predicate PT

      bits<32> target;
      let Inst{127-96} = target;
    }

  def CBRA : GASSInstNoPred<(outs), 
                            (ins VReg1:$a, brtarget:$target),
                            0x947,
                            "@$a BRA \t$target;",
                            [(brcond VReg1:$a, bb:$target)]>, Sched<[]> {
    let Inst{26-23} = 0x7; // predicate PT

    bits<32> target;
    let Inst{127-96} = target;
  }
}

// TODO: This is a dirty fix for tailing bra
def TailBRA : GASSInst<(outs), (ins), 
                       0x947,
                       "// Tail BRA", []>, Sched<[]> {
  let Inst{127-96} = 0xfffffff0;
  let Inst{63-32}  = 0x000fc000;
  let Inst{31-0}   = 0x0383ffff;
}

// BAR.SYNC 0;
def BAR : GASSInst<(outs), (ins), 
                   0xb1d, "BAR.SYNC \t0;", 
                   [(int_nvvm_barrier0)]>, Sched<[]>;

//=---------------------------------=//
// Shuffle
//=---------------------------------=//
// SHFL
// SHFL.BFLY $dst, $src0, $src1, $src2;
//                 var,   laneMask, width (=31 ususally)
// Pattern in C++
let isFixLat = false in {
multiclass SHFL {
  def rrr : GASSInstOp3rrr<(outs VReg32:$dst),
                     (ins VReg32:$src0, VReg32:$src1, VReg32:$src2,
                          ShflMode:$shfl),
                     0x389, "SHFL$shfl $dst, $src0, $src1, $src2;", []>;

  def rri : GASSInstOp3rri<(outs VReg32:$dst),
                    (ins VReg32:$src0, VReg32:$src1, i32imm:$src2,
                        ShflMode:$shfl),
                    0x589, "SHFL$shfl $dst, $src0, $src1, $src2;", []>;

  def rir : GASSInstOp3rir<(outs VReg32:$dst),
                     (ins VReg32:$src0, i32imm:$src1, VReg32:$src2,
                          ShflMode:$shfl),
                     0x989, "SHFL$shfl $dst, $src0, $src1, $src2;", []>;



  def rii : GASSInst<(outs VReg32:$dst),
                     (ins VReg32:$src0, i32imm:$src1, i32imm:$src2,
                          ShflMode:$shfl),
                     0xf89, "SHFL$shfl $dst, $src0, $src1, $src2;", []>;
}
}

defm SHFL : SHFL, Sched<[WriteLD]>;


//=----------------------------------=//
//-------- Conversion -------------
//=----------------------------------=//
// Ref: AMDGPU (AMDGPUInstructions.td:587)
// 32-bit
def : Pat<(v2f16 (bitconvert (i32 VReg32:$src))), (v2f16 VReg32:$src)>;
// 128-bit
// TODO: we need more patterns here
def : Pat<(v8f16 (bitconvert (i128 VReg128:$src))), (v8f16 VReg128:$src)>;
def : Pat<(v8f16 (bitconvert (v2i64 VReg128:$src))), (v8f16 VReg128:$src)>;
// def : Pat<(v8f16 (bitconvert (i128 VReg128:$src))), (MOV128 VReg128:$src)>;
// def : Pat<(v8f16 (bitconvert (i128 imm:$src))), (MOV128 imm:$src)>;

// For constants:
def : Pat<(v4f16 (bitconvert (i64 imm:$src0))), (MOV64i imm:$src0)>;
def MOV128i : GASSPseudoInst<(outs VReg128:$dst), (ins VReg128:$src0),
                             [(set i128:$dst, (imm:$src0))]>, Sched<[WriteINT]>;


//-------- Type Conversion---------
multiclass F2F {
  def _F16_F32 : GASSInstOp1r<(outs VReg32:$dst), (ins VReg32:$src0),
                              0x304, "F2F.F16.F32 \t$dst, $src0;",
                              [(set f16:$dst, (fpround f32:$src0))]>;

  // def _F32_F16rr : 

}
defm F2F : F2F, Sched<[WriteFP32]>; // TODO: maybe SFU


//-------- extract_vector_elt ------
// EXTRACT_VECTOR_ELT -> EXTRACT_SUBREG
// class Extract_Element <> : Pat<>;
def : Pat<(i32 (extractelt v2i32:$src, 0)), (EXTRACT_SUBREG $src, sub0)>;
def : Pat<(i32 (extractelt v2i32:$src, 1)), (EXTRACT_SUBREG $src, sub1)>;
// v4
def : Pat<(i32 (extractelt v4i32:$src, 0)), (EXTRACT_SUBREG $src, sub0)>;
def : Pat<(i32 (extractelt v4i32:$src, 1)), (EXTRACT_SUBREG $src, sub1)>;
def : Pat<(i32 (extractelt v4i32:$src, 2)), (EXTRACT_SUBREG $src, sub2)>;
def : Pat<(i32 (extractelt v4i32:$src, 3)), (EXTRACT_SUBREG $src, sub3)>;

//--------- BUILD_VECTOR -----------
// sub-reg (f16, i16) (TODO: i8, i4, i1)
def : Pat<(v2f16 (build_vector (f16 VReg32:$src0), (f16 VReg32:$src1))),
          (v2f16 (PRMTrir VReg32:$src0, (i32 0x5410), VReg32:$src1))>;
def : Pat<(v2i16 (build_vector (i16 VReg32:$src0), (i16 VReg32:$src1))),
          (v2i16 (PRMTrir VReg32:$src0, (i32 0x5410), VReg32:$src1))>;

//=---------------------------------=//
// Miscellaneous
//=---------------------------------=//
// EXIT, NOP, S2R, CS2R
def EXIT : GASSInst<(outs), (ins), 0x94d, "EXIT;", [(GASSexit)]>,
           Sched<[]> {
  let isTerminator = true;
  let isBarrier = true;
  let hasSideEffects = true;

  let Inst{26-23} = 0x7; // predicate PT
}

def NOP : GASSInst<(outs), (ins), 0x918, "NOP;", []>,
          Sched<[WriteINT]> ;

//=--------------------------------=//
// Read Special Registers
//=--------------------------------=//
// No direct access to S2R
// TODO: SReg encoding
class READ_SREG_R32<string RegName, Intrinsic IntOp, bits<8>RegEnc>
  : GASSInstOp0<(outs VReg32:$dst), (ins), 
                0x919, 
                "S2R \t$dst, "#RegName#";",
                [(set VReg32:$dst, (IntOp))]>, Sched<[WriteLD]> {
  let isFixLat = false;

  // TODO: move this to ISel
  let Inst{15-8} = RegEnc;
}

// ntid.x & nctaid.x are handled in ISel
def READ_TID_X : READ_SREG_R32<"tid.x", int_nvvm_read_ptx_sreg_tid_x, 33>;
def READ_TID_Y : READ_SREG_R32<"tid.y", int_nvvm_read_ptx_sreg_tid_y, 34>;
def READ_TID_Z : READ_SREG_R32<"tid.z", int_nvvm_read_ptx_sreg_tid_z, 35>;
def READ_TID_W : READ_SREG_R32<"tid.w", int_nvvm_read_ptx_sreg_tid_w, 36>;

def READ_CTAID_X : READ_SREG_R32<"ctaid.x", int_nvvm_read_ptx_sreg_ctaid_x, 37>;
def READ_CTAID_Y : READ_SREG_R32<"ctaid.y", int_nvvm_read_ptx_sreg_ctaid_y, 38>;
def READ_CTAID_Z : READ_SREG_R32<"ctaid.z", int_nvvm_read_ptx_sreg_ctaid_z, 39>;
def READ_CTAID_W : READ_SREG_R32<"ctaid.w", int_nvvm_read_ptx_sreg_ctaid_w, 40>;

def READ_LANEID : READ_SREG_R32<"laneid", int_nvvm_read_ptx_sreg_laneid, 0>;

// TODO: SReg: smid, nsmid, gridid, lanemask_eq, 


// def CS2R : GASSInst<>, Sched<[]>;

// For GlobalAddress
def SDTWrapper : SDTypeProfile<1, 1, [SDTCisSameAs<0, 1>, SDTCisPtrTy<0>]>;
def Wrapper    : SDNode<"GASSISD::Wrapper", SDTWrapper>;