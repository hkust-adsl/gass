class GASSInst<dag outOperandList, dag inOperandList, bits<12> opcode, 
               string asmstr, list<dag> pattern> : Instruction {
  let Namespace = "GASS";

  bits<128> Inst;

  dag OutOperandList = outOperandList;
  dag InOperandList = inOperandList;
  let AsmString = asmstr;
  let Pattern = pattern;

  bits<4> PredMask = 7;
  bits<8> Dst;
  bits<8> Src0;
  bits<8> Src1;
  bits<8> Src2;

  // let Inst{103-96} = Src1;
  // let Inst{95-88} = Src0;
  // let Inst{87-80} = Dst;
  let Inst{79-76} = PredMask;
  let Inst{75-64} = opcode;
  // let Inst{7-0} = Src2;
}