// Ref: ARMInstrFormats.td (PredicateOperand)
// Ref: MCInstrDesc::findFirstPredOperandIdx()
def pred : PredicateOperand<OtherVT, (ops i32imm, VReg1), (ops (i32 0), PT)> {
  let PrintMethod = "printPredicateOperand";
  let EncoderMethod = "encodePredicateOperand";
}

class GASSInst<dag oops, dag iops, bits<12> opcode, 
               string asmstr, list<dag> pattern> : Instruction {
  let Namespace = "GASS";

  bits<128> Inst;

  bits<4> pmask;
  bits<8> Dst  = 0;
  bits<8> Src0 = 0;
  bits<8> Src1 = 0;
  bits<8> Src2 = 0;

  let Inst{103-96} = Src1;
  let Inst{95-88}  = Src0;
  let Inst{87-80}  = Dst;
  let Inst{79-76}  = pmask;
  let Inst{75-64}  = opcode;
  let Inst{7-0}    = Src2;

  dag OutOperandList = oops;
  dag InOperandList = !con(iops, (ins pred:$pmask));
  let AsmString = !strconcat("${pmask}\t", asmstr);
  let Pattern = pattern;

  // Schedule Info
  // Need to sync with TsflagsMask in 'GASSInstrInfo.h'
  bit isFixLat = true;
  let TSFlags{0} = isFixLat;

  // Almost all GASS instructions 
  let isPredicable = true;

  // assume instrs have no SideEffects
  let hasSideEffects = false;
}

class GASSInstNoPred<dag oops, dag iops, bits<12> opcode, 
                    string asmstr, list<dag> pattern> : Instruction {
  let Namespace = "GASS";

  bits<128> Inst;

  // dag PredMask = (ins VReg1:$pmask);

  dag OutOperandList = oops;
  dag InOperandList = iops;
  let AsmString = asmstr;
  let Pattern = pattern;

  // bits<4> pmask = 7;
  bits<8> Dst  = 0;
  bits<8> Src0 = 0;
  bits<8> Src1 = 0;
  bits<8> Src2 = 0;

  let Inst{103-96} = Src1;
  let Inst{95-88}  = Src0;
  let Inst{87-80}  = Dst;
  // let Inst{79-76}  = pmask;
  let Inst{75-64}  = opcode;
  let Inst{7-0}    = Src2;

  // Schedule Info
  // Need to sync with TsflagsMask in 'GASSInstrInfo.h'
  bit isFixLat = true;
  let TSFlags{0} = isFixLat;

  let isPredicable = false;
}

class GASSPseudoInst<dag outs, dag ins, list<dag> pattern>
  : GASSInst<outs, ins, 0x0, "", pattern> {
  let isPseudo = true;
}

class GASSPseudoInstNoPred<dag outs, dag ins, list<dag> pattern>
  : GASSInstNoPred<outs, ins, 0x0, "", pattern> {
  let isPseudo = true;
}


//=--------------------------------------------------------------------------=//
// load/store template
//=--------------------------------------------------------------------------=//
class GASSInstLdst<dag outs, dag ins, bits<12> opcode, string asmstr, 
                   list<dag> pattern, int width>
  : GASSInst<outs, ins, opcode, asmstr, pattern> {
  // Width encoding
  bits<3> WidthEncoding = 
    !cond(
      // ignore .s8/.s16 versions
      !eq(width, 8) : 0, 
      !eq(width, 16) : 2,
      !eq(width, 32) : 4,
      !eq(width, 64) : 5,
      !eq(width, 128) : 6,
      true : 4 // default width is .32
    ); // max is 6 
  
  let Inst{11-9} = WidthEncoding;

  bits<8> dst;
  bits<8> ptr;
  bits<8> src1;
  
  let Dst = dst;
  let Src0 = ptr;
  let Src1 = src1;
  
  let isFixLat = false;
}

class GASSInstLdstOff<dag outs, dag ins, bits<12> opcode, string asmstr, 
                   list<dag> pattern, int width>
  : GASSInstLdst<outs, ins, opcode, asmstr, pattern, width> {
  bits<24> offset;

  let Inst{127-104} = offset;
}

class GASSInstLdstGlobal<dag outs, dag ins, bits<12> opcode, string asmstr, 
                   list<dag> pattern, int width>
  : GASSInstLdst<outs, ins, opcode, asmstr, pattern, width> {
  // .E (64-bit version)
  let Inst{8} = 1;
  // cache & scope set as default value (maybe we should expose them?)
  let Inst{21-20} = 1; // 0->.EF; 3->.LU
  let Inst{16-15} = 1; // 0->.CONSTANT; 2->.STRONG; 3->.WEEK
  let Inst{14-13} = 3; // 0->.CTA; 2->.GPU; 3->.SYS

  // Load/Store instructions 
}

class GASSInstLdstGlobalOff<dag outs, dag ins, bits<12> opcode, string asmstr, 
                   list<dag> pattern, int width>
  : GASSInstLdstGlobal<outs, ins, opcode, asmstr, pattern, width> {
  bits<24> offset;

  let Inst{127-104} = offset;
}
//=--------------------------------------------------------------------------=//
// 3-operand template
//=--------------------------------------------------------------------------=//
// examples: iadd3, imad, ffma
class GASSInstOp3rrr<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern = []> 
    : GASSInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;
  bits<8> src0;
  bits<8> src1;
  bits<8> src2;

  let Dst = dst;
  let Src0 = src0;
  let Src1 = src1;
  let Src2 = src2;
}

class GASSInstOp3rri<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern = []> 
    : GASSInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;
  bits<8> src0;
  bits<8> src1;
  bits<32> src2;

  let Dst = dst;
  let Src0 = src0;
  let Src1 = src1;
  let Inst{127-96} = src2;
}

class GASSInstOp3rir<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern = []>
    : GASSInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;
  bits<8> src0;
  bits<32> src1;
  bits<8> src2;

  let Dst = dst;
  let Src0 = src0;
  let Inst{127-96} = src1;
  let Src2 = src2;
}

// TODO: update constant mem width
class GASSInstOp3rrc<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern> 
    : GASSInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;
  bits<8> src0;
  bits<8> src1;
  bits<32> src2;

  let Dst = dst;
  let Src0 = src0;
  let Src1 = src1;
  let Inst{127-102} = src2{25-0};
}

class GASSInstOp3rcr<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern> 
    : GASSInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;
  bits<8> src0;
  bits<32> src1;
  bits<8> src2;

  let Dst = dst;
  let Src0 = src0;
  let Inst{127-102} = src1{25-0};
  let Src2 = src2;
}

//=--------------------------------------------------------------------------=//
// 2-operand template
//=--------------------------------------------------------------------------=//
class GASSInstOp2rr<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern> 
    : GASSInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;
  bits<8> src0;
  bits<8> src1;

  let Dst = dst;
  let Src0 = src0;
  let Src1 = src1;
}

class GASSInstOp2ri<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern> 
    : GASSInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;
  bits<8> src0;
  bits<32> src1;

  let Dst = dst;
  let Src0 = src0;
  let Inst{127-96} = src1;
}

class GASSInstOp2rc<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern> 
    : GASSInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;
  bits<8> src0;
  bits<32> src1;

  let Dst = dst;
  let Src0 = src0;
  let Inst{127-102} = src1{25-0};
}

//=--------------------------------------------------------------------------=//
// 1-operand template
//=--------------------------------------------------------------------------=//
// 1-operand instrs use Src1 to encode $src0
// examples: MOV, F2F
class GASSInstOp1r<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern> 
    : GASSInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;
  bits<8> src0;

  let Dst = dst;
  let Src1 = src0;
}

class GASSInstOp1i<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern> 
    : GASSInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;
  bits<32> src0;

  let Dst = dst;
  let Inst{127-96} = src0;
}

class GASSInstOp1c<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern> 
    : GASSInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;
  bits<32> src0;

  let Dst = dst;
  let Inst{127-102} = src0{25-0};
}

//=--------------------------------------------------------------------------=//
// 0-operand template (dst only)
//=--------------------------------------------------------------------------=//
class GASSInstOp0<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern> 
    : GASSInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;

  let Dst = dst;
}


//==-------------------------------------------------------------------------==//
// Instruction Predicate
//==-------------------------------------------------------------------------==//
def hasSM70 : Predicate<"Subtarget->getSmVersion() >= 70">;
def hasSM75 : Predicate<"Subtarget->getSmVersion() >= 75">;
def hasSM80 : Predicate<"Subtarget->getSmVersion() >= 80">;

//=---------------------------------------------------------------------------=//
// Operands
//=---------------------------------------------------------------------------=//
// ref: AArcht64InstrFormats.td:309
def simm12 : Operand<i16>, ImmLeaf<i16, [{return isInt<12>(Imm);}]>;
def simm20 : Operand<i32>, ImmLeaf<i32, [{return isInt<20>(Imm);}]>;
def simm24 : Operand<i32>, ImmLeaf<i32, [{return isInt<24>(Imm);}]>;

// Constant Memory (e.g., c[0x0][0x160])
// let OperandType = "OPERAND_IMMEDIATE", 
let PrintMethod = "printConstantMem" in {
def i32const : Operand<i32>, TImmLeaf<i32, [{ return isInt<32>;}]>;
def i64const : Operand<i64>, TImmLeaf<i64, [{ return isInt<32>;}]>;
// def f32const : Operand<f32>, ComplexPattern<f32, 1, "SelectConstMem<32>", []>;
// def f64const : Operand<f64>, ComplexPattern<f64, 1, "SelectConstMem<64>", []>;
}

// Uniform Registers
def upred : PredicateOperand<OtherVT, (ops i32imm, SReg1), (ops (i32 0), UPT)> {
  let PrintMethod = "printPredicateOperand";
  let EncoderMethod = "encodePredicateOperand";
}

class GASSUInst<dag oops, dag iops, bits<12> opcode, 
               string asmstr, list<dag> pattern> : Instruction {
  let Namespace = "GASS";

  bits<128> Inst;

  bits<4> pmask;
  bits<8> Dst  = 0;
  bits<8> Src0 = 0;
  bits<8> Src1 = 0;
  bits<8> Src2 = 0;

  let Inst{103-96} = Src1;
  let Inst{95-88}  = Src0;
  let Inst{87-80}  = Dst;
  let Inst{79-76}  = pmask;
  let Inst{75-64}  = opcode;
  let Inst{7-0}    = Src2;

  dag OutOperandList = oops;
  dag InOperandList = !con(iops, (ins upred:$pmask));
  let AsmString = !strconcat("${pmask}\t", asmstr);
  let Pattern = pattern;

  // Schedule Info
  // Need to sync with TsflagsMask in 'GASSInstrInfo.h'
  bit isFixLat = true;
  let TSFlags{0} = isFixLat;

  // Almost all GASS instructions 
  let isPredicable = true;

  // assume instrs have no SideEffects
  let hasSideEffects = false;
}
//=--------------------------------------------------------------------------=//
// 3-operand template
//=--------------------------------------------------------------------------=//
class GASSUInstOp3rrr<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern> 
    : GASSUInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;
  bits<8> src0;
  bits<8> src1;
  bits<8> src2;

  let Dst = dst;
  let Src0 = src0;
  let Src1 = src1;
  let Src2 = src2;
}

class GASSUInstOp3rri<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern> 
    : GASSUInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;
  bits<8> src0;
  bits<8> src1;
  bits<32> src2;

  let Dst = dst;
  let Src0 = src0;
  let Src1 = src1;
  let Inst{127-96} = src2;

  let Inst{27} = 1; // UR as operand
}

class GASSUInstOp3rir<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern> 
    : GASSUInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;
  bits<8> src0;
  bits<32> src1;
  bits<8> src2;

  let Dst = dst;
  let Src0 = src0;
  let Inst{127-96} = src1;
  let Src2 = src2;

  let Inst{27} = 1; // UR as operand
}

//=--------------------------------------------------------------------------=//
// 2-operand template
//=--------------------------------------------------------------------------=//
class GASSUInstOp2rr<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern> 
    : GASSUInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;
  bits<8> src0;
  bits<8> src1;

  let Dst = dst;
  let Src0 = src0;
  let Src1 = src1;

  let Inst{27} = 1; // UR as operand
}

class GASSUInstOp2ri<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern> 
    : GASSUInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;
  bits<8> src0;
  bits<32> src1;

  let Dst = dst;
  let Src0 = src0;
  let Inst{127-96} = src1;

  let Inst{27} = 1; // UR as operand
}

//=--------------------------------------------------------------------------=//
// 1-operand template
//=--------------------------------------------------------------------------=//
class GASSUInstOp1r<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern> 
    : GASSUInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;
  bits<8> src0;

  let Dst = dst;
  let Src1 = src0;

  let Inst{27} = 1; // UR as operand
}

class GASSUInstOp1i<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern> 
    : GASSUInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;
  bits<32> src0;

  let Dst = dst;
  let Inst{127-96} = src0;
}

//=--------------------------------------------------------------------------=//
// 0-operand template (dst only)
//=--------------------------------------------------------------------------=//
class GASSUInstOp0<dag oops, dag iops, bits<12> opcode, string asm, 
                  list<dag> pattern> 
    : GASSUInst<oops, iops, opcode, asm, pattern> {
  bits<8> dst;

  let Dst = dst;
}

class GASSPseudoUInst<dag outs, dag ins, list<dag> pattern>
  : GASSUInst<outs, ins, 0x0, "", pattern> {
  let isPseudo = true;
}
