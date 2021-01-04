//=------------------------------------=//
// Format
//=------------------------------------=//
class GASSInst<dag outs, dag ins, bits<12> opcode, 
               string asmstr, list<dag> pattern> : Instruction {
  let Namespace = "GASS";

  bits<128> Inst;

  dag OutOperandList = outs;
  dag InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;

  bits<4> PredMask;
  bits<8> Dst;
  bits<8> Src0;
  bits<8> Src1;
  bits<8> Src2;

  // let Inst{103-96} = Src1;
  // let Inst{95-88} = Src0;
  // let Inst{87-80} = Dst;
  let Inst{75-64} = opcode;
  // let Inst{7-0} = Src2;
}

//=---------------------------------=//
// float-point
//=---------------------------------=//
multiclass FFMA<string OpcStr, RegisterClass RC, bits<12> Enc> {
  def rr : GASSInst<(outs RC:$dst),
                    (ins RC:$a, RC:$b, RC:$c),
                    Enc, OpcStr, 
                    [(set f32:$dst, (fma f32:$a, f32:$b, f32:$c))]>;
}

defm FFMA : FFMA<"FFMA", VReg32, 0x223>;

//=---------------------------------=//
// Misc
//=---------------------------------=//
def EXIT : GASSInst<(outs), (ins), 0x94d, "EXIT", []> ; // ret void?
def NOP : GASSInst<(outs), (ins), 0x918, "NOP", []> ; // maybe @llvm.donothing()