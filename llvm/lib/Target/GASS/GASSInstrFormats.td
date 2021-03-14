def pred : PredicateOperand<i1, (ops VReg1), (ops PT)> {
  let PrintMethod = "printPredicateOperand";
}

class GASSInst<dag oops, dag iops, bits<12> opcode, 
               string asmstr, list<dag> pattern> : Instruction {
  let Namespace = "GASS";

  bits<128> Inst;

  // dag PredMask = (ins VReg1:$pmask);

  dag OutOperandList = oops;
  dag InOperandList = !con(iops, (ins pred:$pmask));
  let AsmString = !strconcat("${pmask}\t", asmstr);
  let Pattern = pattern;

  bits<4> pmask = 7;
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

  // Schedule Info
  // Need to sync with TsflagsMask in 'GASSInstrInfo.h'
  bit isFixLat = true;
  let TSFlags{0} = isFixLat;

  // Almost all GASS instructions 
  let isPredicable = true;
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
                  list<dag> pattern> 
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
                  list<dag> pattern> 
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
                  list<dag> pattern> 
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