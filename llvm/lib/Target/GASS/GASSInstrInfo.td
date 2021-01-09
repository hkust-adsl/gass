//=------------------------------------=//
// Format
//=------------------------------------=//
include "GASSInstrFormats.td"

// SDNode
def GASSexit : SDNode<"GASSISD::EXIT", SDTNone, 
    [SDNPHasChain, SDNPOptInGlue]>; // AMDGPU does this.

def GASSldc : SDNode<"GASSISD::LDC", SDTLoad, []>; // Constant load

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
// load/store instructions
//=---------------------------------=//
multiclass LDC<RegisterClass RC, int Width> {
  def c : GASSInst<(outs RC:$dst),
                   (ins i16imm:$offset),
                   0xb82,
                   "LDC." #Width #" \t$dst, [$offset];",
                   []>; 

  // def rc : ;
}

defm LDC32 : LDC<VReg32, 32>;
defm LDC64 : LDC<VReg64, 64>;

multiclass LDG<RegisterClass RC, int Width> {
  def r : GASSInst<(outs RC:$dst),
                   (ins VReg64:$ptr),
                   0x381,
                   "LDG."#Width#" \t$dst, [$ptr];",
                   []>;

  def ri : GASSInst<(outs RC:$dst),
                    (ins VReg64:$ptr, i16imm:$offset),
                    0x381,
                    "LDG."#Width#" \t$dst, [$ptr+$offset];",
                    []>;
}

defm LDG32 : LDG<VReg32, 32>;
defm LDG64 : LDG<VReg64, 64>;


multiclass STG<RegisterClass RC, int Width> {
  def r : GASSInst<(outs),
                   (ins RC:$src, VReg64:$ptr), // RZ?
                   0x386,
                   "STG."#Width#" \t[$ptr], $src;",
                   []>;
  
  def ri : GASSInst<(outs),
                    (ins RC:$src, VReg64:$ptr, i16imm:$offset),
                    0x386,
                    "STG."#Width#" \t[$ptr+$offset], $src;",
                    []>;
}

defm STG32 : STG<VReg32, 32>;
defm STG64 : STG<VReg64, 64>;

//=---------------------------------=//
// Misc
//=---------------------------------=//
def EXIT : GASSInst<(outs), (ins), 0x94d, "EXIT;", [(GASSexit)]> {
  let isTerminator = true;
  let isBarrier = true;
  let hasSideEffects = true;
}

def NOP : GASSInst<(outs), (ins), 0x918, "NOP;", []> ; // maybe @llvm.donothing()