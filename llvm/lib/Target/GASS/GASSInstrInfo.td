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

// .xor .or .and
def BoolMode : Operand<i32> {
  let PrintMethod = "printBoolMode";
  let EncoderMethod = "encodeBoolMode";
}

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

//=---------------------------------=//
// Integer
//=---------------------------------=//
// IADD3, IMUL, IMAD, SHF, LOP3
multiclass IADD3 {
  def rrr : GASSInst<(outs VReg32:$dst),
                     (ins VReg32:$src0, VReg32:$src1, VReg32:$src2),
                     0x210,
                     "IADD3 \t$dst, $src0, $src1, $src2;",
                     [(set i32:$dst, (add (add i32:$src0, i32:$src1), 
                                          i32:$src2))]>;

  def rri : GASSInst<(outs VReg32:$dst),
                     (ins VReg32:$src0, VReg32:$src1, i32imm:$src2),
                     0x410,
                     "IADD3 \t$dst, $src0, $src1, $src2;",
                     [(set i32:$dst, (add (add i32:$src0, i32:$src1),
                                          imm:$src2))]>;

  // Consider this
  // def _Xrrr : ;
}
defm IADD3 : IADD3, Sched<[WriteINT]>;
// Support 2-source add (TODO)
def : Pat<(i32 (add i32:$src0, i32:$src1)),
          (IADD3rri VReg32:$src0, VReg32:$src1, PatLeaf<(i32 0)>)>;

// 2-operand IADD, 
// This is IADD3 with default value
multiclass IADD {
  def rr : GASSInst<(outs VReg32:$dst),
                    (ins VReg32:$src0, VReg32:$src1),
                    0x210,
                    "IADD3 \t$dst, $src0, $src1, RZ;",
                    [(set i32:$dst, (add i32:$src0, i32:$src1))]>;
  
  def ri : GASSInst<(outs VReg32:$dst),
                    (ins VReg32:$src0, i32imm:$src1),
                    0x810,
                    "IADD3 \t$dst, $src0, $src1, RZ;",
                    [(set i32:$dst, (add i32:$src0, imm:$src1))]>;
}
defm IADD : IADD, Sched<[WriteINT]>;

// lhs/rhs ADDX (ADDCARRY)
// This is (real) IADD3 with defalut value
multiclass IADDX {
  def rr : GASSInst<(outs VReg32:$dst, VReg1:$carryout),
                    (ins VReg32:$src0, VReg32:$src1, VReg1:$carryin),
                    0x210,
                    "IADD3.X \t$dst, $carryout, $src0, $src1, RZ, $carryin",
                    [/*TODO*/]>;

  def ri : GASSInst<(outs VReg32:$dst, VReg1:$carryout),
                    (ins VReg32:$src0, i32imm:$src1, VReg1:$carryin),
                    0x810,
                    "IADD3.X \t$dst, $carryout, $src0, $src1, RZ, $carryin",
                    [/*TODO*/]>;
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

// IMUL
// Pseudo instruction
let isPseudo = true in {
multiclass IMUL {
  def rr : GASSInst<(outs VReg32:$dst),
                    (ins VReg32:$src0, VReg32:$src1),
                    0x0, "", 
                    [(set i32:$dst, (mul i32:$src0, i32:$src1))]>;

  def _WIDErr : GASSInst<(outs VReg64:$dst),
                         (ins VReg32:$src0, VReg32:$src1),
                         0x0, "",
                         []>;

  def _WIDEri : GASSInst<(outs VReg64:$dst),
                         (ins VReg32:$src0, i32imm:$src1),
                         0x0, "",
                         []>;
}
} // isPseudo = true
defm IMUL : IMUL, Sched<[WriteINT]>;

// IMAD
multiclass IMAD<bit IsSigned> {
  def rrr : GASSInst<(outs VReg32:$dst),
                     (ins VReg32:$src0, VReg32:$src1, VReg32:$src2),
                     0x224,
                     "IMAD \t$dst, $src0, $src1, $src2;",
                     [(set i32:$dst, (add (mul i32:$src0, i32:$src1), 
                                          i32:$src2))]>;

  def rri : GASSInst<(outs VReg32:$dst),
                     (ins VReg32:$src0, VReg32:$src1, i32imm:$src2),
                     0x424,
                     "IMAD \t$dst, $src0, $src1, $src2;",
                     [(set i32:$dst, (add (mul i32:$src0, i32:$src1),
                                          imm:$src2))]>;

  def rir : GASSInst<(outs VReg32:$dst),
                     (ins VReg32:$src0, i32imm:$src1, VReg32:$src2),
                     0x824,
                     "IMAD \t$dst, $src0, $src1, $src2;",
                     [(set i32:$dst, (add (mul i32:$src0, imm:$src1),
                                          i32:$src2))]>;

  def _WIDErrr : GASSInst<(outs VReg64:$dst),
                          (ins VReg32:$src0, VReg32:$src1, VReg64:$src2),
                          0x225,
                          "IMAD.WIDE \t$dst, $src0, $src1, $src2;",
                          // Does this work?
                          []>;

  // def _WIDErri : ;

  def _WIDErir : GASSInst<(outs VReg64:$dst),
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
//   Lo32bits, shiftAmount, Hi32Bits, l/r, lo/hi, type
multiclass SHF {
  def rrr : GASSInst<(outs VReg32:$dst),
                       (ins VReg32:$src0, VReg32:$src1, VReg32:$src2),
                       0x219,
                       "SHF \t$dst, $src0, $src1, $src2;",
                       []>;

  def rri : GASSInst<(outs VReg32:$dst),
                       (ins VReg32:$src0, VReg32:$src1, i32imm:$src2),
                       0x419,
                       "SHF \t$dst, $src0, $src1, $src2;",
                       []>;

  def rir : GASSInst<(outs VReg32:$dst),
                     (ins VReg32:$src0, VReg32:$src1, i32imm:$src2),
                     0x819,
                     "SHF \t$dst, $src0, $src1, $src2;",
                     []>;
}
defm SHF : SHF, Sched<[WriteINT]>;

// LOP3
//   LOP3 $dst, $src1, $src2, $src3, $immLut;
multiclass LOP3 {
  def rrr : GASSInst<(outs VReg32:$dst),
                     (ins VReg32:$src0, VReg32:$src1, VReg32:$src2, 
                          i8imm:$immLut),
                     0x212,
                     "LOP3.LUT $dst, $src0, $src1, $src2, $immLut;",
                     []>;

  def rri : GASSInst<(outs VReg32:$dst),
                     (ins VReg32:$src0, VReg32:$src1, i32imm:$src2,
                          i8imm:$immLut),
                     0x412,
                     "LOP3.LUT $dst, $src0, $src1, $src2, $immLut;",
                     []>;

  def rir : GASSInst<(outs VReg32:$dst),
                     (ins VReg32:$src0, i32imm:$src1, VReg32:$src2,
                          i8imm:$immLut),
                     0x812,
                     "LOP3.LUT $dst, $src0, $src1, $src2, $immLut;",
                     []>;

  // def rrc : ;
}
defm LOP3 : LOP3, Sched<[WriteINT]>;

// TODO: combine xor/or/and/not
//==------------------------------------------------------------------------==//
// xor/or/and/not (all pseudo)
//==------------------------------------------------------------------------==//
multiclass BITWISE<SDNode OpNode> {
  def rr : GASSPseudoInst<(outs VReg32:$dst),
                          (ins VReg32:$src0, VReg32:$src1),
                          [(set i32:$dst, (OpNode i32:$src0, i32:$src1))]>,
           Sched<[WriteINT]>;

  def ri : GASSPseudoInst<(outs VReg32:$dst),
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
multiclass SHL64 {
  def rr : GASSInst<(outs VReg64:$dst),
                          (ins VReg64:$src0, VReg64:$src1), 
                          0x0, 
                          "SHL.64 \t$dst, $src0, $src1; // FIXME FIXME Pseudo",
                          [(set i64:$dst, (shl i64:$src0, i64:$src1))]>;

  def ri : GASSInst<(outs VReg64:$dst),
                          (ins VReg64:$src0, i64imm:$src1),
                          0x0, 
                          "SHL.64 \t$dst, $src0, $src1; // FIXME FIXME Pseudo",
                          [(set i64:$dst, (shl i64:$src0, (i64 imm:$src1)))]>;
}
defm SHL64 : SHL64, Sched<[WriteINT]>;

// SignExtend
// i32->i64
class SEXT : GASSPseudoInst<(outs VReg64:$dst),
                            (ins VReg32:$src0),
                            [(set i64:$dst, (sext i32:$src0))]>;
def SEXT : SEXT, Sched<[WriteINT]>;

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

// // LOP3
// multiclass LOP3 {
//   def rrr;
// }

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
  def rrr : GASSInst<(outs RC:$dst),
                    (ins RC:$a, RC:$b, RC:$c),
                    rrrEnc, 
                    !strconcat(OpcStr, " \t$dst, $a, $b, $c;"), 
                    [(set vt:$dst, (fma vt:$a, vt:$b, vt:$c))]>;
  
  def rri : GASSInst<(outs RC:$dst),
                     (ins RC:$a, RC:$b, ImmCls:$c),
                     rriEnc,
                     !strconcat(OpcStr, " \t$dst, $a, $b, $c;"),
                     [(set vt:$dst, (fma vt:$a, vt:$b, fpimm:$c))]>;

  def rir : GASSInst<(outs RC:$dst),
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
  def rr : GASSInst<(outs RC:$dst),
                    (ins RC:$a, RC:$b),
                    rrEnc,
                    !strconcat(OpcStr, " \t$dst, $a, $b;"),
                    [(set vt:$dst, (fmul vt:$a, vt:$b))]>;
  
  def ri : GASSInst<(outs RC:$dst),
                     (ins RC:$a, ImmCls:$b),
                     riEnc,
                     !strconcat(OpcStr, " \t$dst, $a, $b;"),
                     [(set vt:$dst, (fmul vt:$a, fpimm:$b))]>;
}

// ADD
multiclass ADD<string OpcStr, RegisterClass RC, Operand ImmCls, ValueType vt,
               bits<12> rrEnc, bits<12> riEnc> {
  def rr : GASSInst<(outs RC:$dst),
                    (ins RC:$a, RC:$b),
                    rrEnc,
                    !strconcat(OpcStr, " \t$dst, $a, $b;"),
                    [(set vt:$dst, (fadd vt:$a, vt:$b))]>;
  
  def ri : GASSInst<(outs RC:$dst),
                     (ins RC:$a, ImmCls:$b),
                     riEnc,
                     !strconcat(OpcStr, " \t$dst, $a, $b;"),
                     [(set vt:$dst, (fadd vt:$a, fpimm:$b))]>;
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
  def rr : GASSInst<(outs RC:$dst), 
                    (ins RC:$a, RC:$b, RC:$c),
                    rrrEnc,
                    "HFMA2 \t$dst, $a, $b, $c;",
                    [/*TODO*/]>;
}

multiclass HADD2<RegisterClass RC, bits<12> rrrEnc> {
  def rr : GASSInst<(outs RC:$dst),
                    (ins RC:$a, RC:$b),
                    rrrEnc,
                    "HADD2 \t$dst, $a, $b;",
                    [/*TODO*/]>;
}

multiclass HMUL2<RegisterClass RC, bits<12> rrrEnc> {
  def rr : GASSInst<(outs RC:$dst),
                    (ins RC:$a, RC:$b),
                    rrrEnc,
                    "HMUL2 \t$dst, $a, $b;",
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
  def rr : GASSInst<(outs VReg1:$dst), // TODO: 2 dsts (no?)
                    (ins RC:$src0, RC:$src1, CmpMode:$cmp),
                    rrEnc,
                    !strconcat(OpcStr, "$cmp \t$dst, $src0, $src1;"),
                    []>;

  def ri : GASSInst<(outs VReg1:$dst),
                    (ins RC:$src0, ImmCls:$src1, CmpMode:$cmp),
                    riEnc,
                    !strconcat(OpcStr, "$cmp \t$dst, $src0, $src1"),
                    []>;
}
defm ISETP : SETP<"ISETP", VReg32, i32imm, 0x20c, 0x40c>, Sched<[WriteINT]>;

// Patterns
def : Pat<(i1 (seteq i32:$a, i32:$b)), (ISETPrr VReg32:$a, VReg32:$b, CmpEQ)>;
def : Pat<(i1 (seteq i32:$a, imm:$b)), (ISETPri VReg32:$a, imm:$b, CmpEQ)>;
def : Pat<(i1 (setne i32:$a, i32:$b)), (ISETPrr VReg32:$a, VReg32:$b, CmpNE)>;
def : Pat<(i1 (setne i32:$a, imm:$b)), (ISETPri VReg32:$a, imm:$b, CmpNE)>;
def : Pat<(i1 (setlt i32:$a, i32:$b)), (ISETPrr VReg32:$a, VReg32:$b, CmpLT)>;
def : Pat<(i1 (setlt i32:$a, imm:$b)), (ISETPri VReg32:$a, imm:$b, CmpLT)>;
def : Pat<(i1 (setle i32:$a, i32:$b)), (ISETPrr VReg32:$a, VReg32:$b, CmpLE)>;
def : Pat<(i1 (setle i32:$a, imm:$b)), (ISETPri VReg32:$a, imm:$b, CmpLE)>;
def : Pat<(i1 (setgt i32:$a, i32:$b)), (ISETPrr VReg32:$a, VReg32:$b, CmpGT)>;
def : Pat<(i1 (setgt i32:$a, imm:$b)), (ISETPri VReg32:$a, imm:$b, CmpGT)>;
def : Pat<(i1 (setge i32:$a, i32:$b)), (ISETPrr VReg32:$a, VReg32:$b, CmpGE)>;
def : Pat<(i1 (setge i32:$a, imm:$b)), (ISETPri VReg32:$a, imm:$b, CmpGE)>;


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
// should have direct intrinsic mapping
// multiclass HMMA {
//   def F32_1688 : ;

//   def F16_1688 : ;
// }
// defm HMMA : HMMA, Sched<[WriteTC]>;

// multiclass IMMA {
// }
// defm IMMA : IMMA, Sched<[WriteTC]>;

// multiclass BMMA {
// }
// defm BMMA : BMMA, Sched<[WriteTC]>;

//=---------------------------------=//
// mov
//=---------------------------------=//
multiclass MOV<RegisterClass RC, int Width> {
  def r : GASSInst<(outs RC:$dst),
                   (ins RC:$src),
                   0x202,
                   "MOV."#Width#" \t$dst, $src;",
                   []>;

  def i : GASSInst<(outs RC:$dst),
                   (ins i32imm:$src),
                   0x202,
                   "MOV."#Width#" \t$dst, $src;",
                   []>;
  
  def f : GASSInst<(outs RC:$dst),
                   (ins f32imm:$src),
                   0x202,
                   "MOV."#Width#" \t$dst, $src;",
                   []>;

  def c : GASSInst<(outs RC:$dst),
                   (ins RC:$src), // Not true.
                   0x202,
                   "MOV."#Width#" \t$dst, $src;",
                   []>;
}

defm MOV32 : MOV<VReg32, 32>, Sched<[WriteLD]>;
defm MOV64 : MOV<VReg64, 64>, Sched<[WriteLD]>; // Really need to support this?

// TODO: this can't be right?
def : Pat<(i32 (imm:$src0)),   (MOV32i imm:$src0)>;
def : Pat<(f32 (fpimm:$src0)), (MOV32f fpimm:$src0)>;

//=---------------------------------=//
// load/store instructions
//=---------------------------------=//
// LDC, LDG, STG
// TODO: LD, LDS, LDSM, LDL, ST, STS
multiclass LDC<RegisterClass RC, int Width> {
  def c : GASSInstLdst<(outs RC:$dst),
                       (ins constantmem:$offset), 
                       0xb82,
                       "LDC." #Width #" \t$dst, $offset;",
                       [],
                       Width> {
    let ptr = 0xff; // base offset
    bits<32> offset; // TODO: check width of constant mem
    let Inst{127-102} = offset{25-0}; 
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
                       "LDG."#Width#" \t$dst, [$ptr];",
                       [],
                       Width> {
    let Inst{20-17} = 0x7; // predicate: PT 
  }

  def ri : GASSInstLdstGlobal<(outs RC:$dst),
                    (ins VReg64:$ptr, i16imm:$offset),
                    0x381,
                    "LDG."#Width#" \t$dst, [$ptr+$offset];",
                    [],
                    Width> {
    let Inst{20-17} = 0x7; // predicate: PT
  }
}
}

defm LDG32 : LDG<VReg32, 32>, Sched<[WriteLD]>;
defm LDG64 : LDG<VReg64, 64>, Sched<[WriteLD]>;


multiclass STG<RegisterClass RC, int Width> {
  def r : GASSInstLdstGlobal<(outs),
                       (ins RC:$src, VReg64:$ptr), // RZ?
                       0x386,
                       "STG."#Width#" \t[$ptr], $src;",
                       [],
                       Width>;
  
  def ri : GASSInstLdstGlobal<(outs),
                        (ins RC:$src, VReg64:$ptr, i16imm:$offset),
                        0x386,
                        "STG."#Width#" \t[$ptr+$offset], $src;",
                        [],
                        Width>;
}

defm STG32 : STG<VReg32, 32>, Sched<[WriteST]>;
defm STG64 : STG<VReg64, 64>, Sched<[WriteST]>;


//=---------------------------------=//
// Control Flow
//=---------------------------------=//
// BRA, CBRA (condition branch), BAR,
// TODO: BSSY, BSYNC, WARPSYNC (?)
let isTerminator = true, isBranch = true in {
  let isBarrier = true in 
    def BRA : GASSInst<(outs), 
                      (ins brtarget:$target), 
                      0x947, 
                      "BRA \t$target;", 
                      [(br bb:$target)]>, Sched<[]>;

  def CBRA : GASSInst<(outs), 
                      (ins VReg1:$a, brtarget:$target),
                      0x947,
                      "@$a BRA \t$target;",
                      [(brcond VReg1:$a, bb:$target)]>, Sched<[]>;
}

// BAR.SYNC 0;
def BAR : GASSInst<(outs), (ins), 
                   0xb1d, "BAR.SYNC \t0;", 
                   [(int_nvvm_barrier0)]>, Sched<[]>;

//=---------------------------------=//
// Shuffle
//=---------------------------------=//
// SHFL



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
          Sched<[]> { // maybe @llvm.donothing()
  let Inst{26-23} = 0x7; // predicate PT
}

//=--------------------------------=//
// Read Special Registers
//=--------------------------------=//
// No direct access to S2R
// TODO: SReg encoding
class READ_SREG_R32<string RegName, Intrinsic IntOp>
  : GASSInst<(outs VReg32:$dst), (ins), 
             0x919, 
             "S2R \t$dst, "#RegName#";",
             [(set VReg32:$dst, (IntOp))]>, Sched<[WriteLD]> {
  let isFixLat = false;
}

def READ_TID_X : READ_SREG_R32<"tid.x", int_nvvm_read_ptx_sreg_tid_x>;
def READ_TID_Y : READ_SREG_R32<"tid.y", int_nvvm_read_ptx_sreg_tid_y>;
def READ_TID_Z : READ_SREG_R32<"tid.z", int_nvvm_read_ptx_sreg_tid_z>;
def READ_TID_W : READ_SREG_R32<"tid.w", int_nvvm_read_ptx_sreg_tid_w>;

def READ_NTID_X : READ_SREG_R32<"ntid.x", int_nvvm_read_ptx_sreg_ntid_x>;
def READ_NTID_Y : READ_SREG_R32<"ntid.y", int_nvvm_read_ptx_sreg_ntid_y>;
def READ_NTID_Z : READ_SREG_R32<"ntid.z", int_nvvm_read_ptx_sreg_ntid_z>;
def READ_NTID_W : READ_SREG_R32<"ntid.w", int_nvvm_read_ptx_sreg_ntid_w>;

def READ_CTAID_X : READ_SREG_R32<"ctaid.x", int_nvvm_read_ptx_sreg_ctaid_x>;
def READ_CTAID_Y : READ_SREG_R32<"ctaid.y", int_nvvm_read_ptx_sreg_ctaid_y>;
def READ_CTAID_Z : READ_SREG_R32<"ctaid.z", int_nvvm_read_ptx_sreg_ctaid_z>;
def READ_CTAID_W : READ_SREG_R32<"ctaid.w", int_nvvm_read_ptx_sreg_ctaid_w>;

def READ_NCTAID_X : READ_SREG_R32<"nctaid.x", int_nvvm_read_ptx_sreg_nctaid_x>;
def READ_NCTAID_Y : READ_SREG_R32<"nctaid.y", int_nvvm_read_ptx_sreg_nctaid_y>;
def READ_NCTAID_Z : READ_SREG_R32<"nctaid.z", int_nvvm_read_ptx_sreg_nctaid_z>;
def READ_NCTAID_W : READ_SREG_R32<"nctaid.w", int_nvvm_read_ptx_sreg_nctaid_w>;

def READ_LANEID : READ_SREG_R32<"laneid", int_nvvm_read_ptx_sreg_laneid>;
def READ_WARPID : READ_SREG_R32<"warpid", int_nvvm_read_ptx_sreg_warpid>;
def READ_NWARPID : READ_SREG_R32<"nwarpid", int_nvvm_read_ptx_sreg_nwarpid>;

// TODO: SReg: smid, nsmid, gridid, lanemask_eq, 


// def CS2R : GASSInst<>, Sched<[]>;