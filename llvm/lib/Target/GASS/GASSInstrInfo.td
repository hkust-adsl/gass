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

defm FFMA : FFMA<"FFMA", VReg32, 0x223>, Sched<[WriteFP32]>;


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
                   (ins RC:$src),
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

//=---------------------------------=//
// load/store instructions
//=---------------------------------=//
multiclass LDC<RegisterClass RC, int Width> {
  def c : GASSInstLdst<(outs RC:$dst),
                       (ins constantmem:$offset), 
                       0xb82,
                       "LDC." #Width #" \t$dst, $offset;",
                       [],
                       Width> {
    bits<32> offset; // TODO: check width of constant mem
    let Inst{127-96} = offset; 
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
// Misc
//=---------------------------------=//
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