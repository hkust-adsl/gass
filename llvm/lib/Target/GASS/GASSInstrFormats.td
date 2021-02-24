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

class GASSInstLdst<dag outs, dag ins, bits<12> opcode, string asmstr, 
                   list<dag> pattern, int width>
  : GASSInst<outs, ins, opcode, asmstr, pattern> {
  // Width encoding
  bits<3> WidthEncoding = 
    !cond(
      // ignore .u8/.u16 versions
      !eq(width, 8) : 1, 
      !eq(width, 16) : 3,
      !eq(width, 32) : 4,
      !eq(width, 64) : 5,
      !eq(width, 128) : 6,
      true : 4 // default width is .32
    ); // max is 6 
  
  let Inst{11-9} = WidthEncoding;

  bits<8> dst;
  bits<8> ptr;
  
  let Dst = dst;
  let Src0 = ptr;
  
  let isFixLat = false;
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