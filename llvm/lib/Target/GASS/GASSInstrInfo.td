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

defm MOV32 : MOV<VReg32, 32>;
defm MOV64 : MOV<VReg64, 64>; // Really need to support this?

//=---------------------------------=//
// load/store instructions
//=---------------------------------=//
multiclass LDC<RegisterClass RC, int Width> {
  def c : GASSInstLdst<(outs RC:$dst),
                       (ins i32imm:$offset),
                       0xb82,
                       "LDC." #Width #" \t$dst, [$offset];",
                       [],
                       Width>;

  // def rc : ;
}

defm LDC32 : LDC<VReg32, 32>;
defm LDC64 : LDC<VReg64, 64>;

multiclass LDG<RegisterClass RC, int Width> {
  def r : GASSInstLdstGlobal<(outs RC:$dst),
                       (ins VReg64:$ptr),
                       0x381,
                       "LDG."#Width#" \t$dst, [$ptr];",
                       [],
                       Width>;

  def ri : GASSInstLdstGlobal<(outs RC:$dst),
                    (ins VReg64:$ptr, i16imm:$offset),
                    0x381,
                    "LDG."#Width#" \t$dst, [$ptr+$offset];",
                    [],
                    Width>;
}

defm LDG32 : LDG<VReg32, 32>;
defm LDG64 : LDG<VReg64, 64>;


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