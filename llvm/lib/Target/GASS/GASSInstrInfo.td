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

//=---------------------------------=//
// Integer
//=---------------------------------=//
// IADD3, IMUL, IMAD, SHF, LOP3
// multiclass IADD3 {
//   // 0x210
//   def rrr : ;

//   def rri : ;

//   def rrc : ;

//   // Consider this
//   def _Xrrr : ;
// }
// defm IADD3 : IADD3, Sched<[WriteINT]>;

// // IMUL
// // TODO

// // IMAD
// multiclass IMAD<bit IsSigned> {
//   def rrr : ;

//   def rri : ;

//   def rir : ;

//   def rrc : ;

//   def rcr : ;

//   def _WIDErrr : ;

//   def _WIDErri : ;

//   def _WIDErir : ;

//   def _WIDErrc : ;

//   def _WIDErcr : ;

//   // TODO: fill this.
//   // def _HIrrr : ;
// }
// defm IMAD_S32 : IMAD<true>, Sched<[WriteFP32]>;
// defm IMAD_U32 : IMAD<false>, Sched<[WriteFP32]>;

// // SHF
// multiclass SHF {
//   def _Lrrr : ;

//   def _Rrrr : ;
// }
// defm SHF_S32 : IMAD<true>, Sched<[WriteINT]>;
// defm SHF_U32 : IMAD<false>, Sched<[WriteINT]>;

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
// class SETP : GASSInst<(outs), >;


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
             [(set VReg32:$dst, (IntOp))]>, Sched<[WriteLD]>;

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