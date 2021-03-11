class GASSReg<string n, bits<16> enc> : Register<n> {
  let Namespace = "GASS";
  let HWEncoding = enc;
}

class GASSRegClass<list<ValueType> regTypes, int alignment, dag regList>
  : RegisterClass<"GASS", regTypes, alignment, regList>;

let Namespace = "GASS" in {
  def sub0 : SubRegIndex<32>;
  def sub1 : SubRegIndex<32>;
  def sub2 : SubRegIndex<32>;
  def sub3 : SubRegIndex<32>;
}

foreach Index = 0-64 in {
  def SGPR#Index : GASSReg<"UR"#Index, Index>;
}

foreach Index = 0-256 in {
  def VGPR#Index : GASSReg<"R"#Index, Index>;
}

foreach Index = 0-7 in {
  def PR#Index : GASSReg<"P"#Index, Index>;
}

// Zero registers
def PT   : GASSReg<"PT", 0x7>;
def NPT  : GASSReg<"!PT", 0xf>;
def RZ32 : GASSReg<"RZ", 0xff>;
def RZ32_1 : GASSReg<"RZ32_1", 0xff>; // requred by RZ64, should never be used.
def RZ64 : GASSReg<"RZ", 0xff> {
  let SubRegIndices = [sub0, sub1];
  let SubRegs = [RZ32, RZ32_1];
}

// Vector registers
// Following the practice in AMDGPU (SIRegisterInfo.td)
// Generates list of sequential register tuple names.
// E.g. RegSeq<3,2,2,"s">.ret -> [ "s[0:1]", "s[2:3]" ]
class RegSeqNames<int last_reg, int stride, int size, string prefix,
                  int start = 0> {
  int next = !add(start, stride);
  int end_reg = !add(start, size, -1);
  list<string> ret =
    !if(!le(end_reg, last_reg),
        !listconcat([prefix # "[" # start # ":" # end_reg # "]"],
                    RegSeqNames<last_reg, stride, size, prefix, next>.ret),
                    []);
}

// Generates list of dags for register tupless.
class RegSeqDags<RegisterClass RC, int last_reg, int stride, int size,
                int start = 0> {
  dag trunc_rc = (trunc RC,
                  !if(!and(!eq(stride, 1), !eq(start, 0)),
                      !sub(!add(last_reg, 2), size),
                      !add(last_reg, 1)));
  list<dag> ret =
    !if(!lt(start, size),
        !listconcat([(add (decimate (shl trunc_rc, start), stride))],
                    RegSeqDags<RC, last_reg, stride, size, !add(start, 1)>.ret),
        []);
}

class GASSRegTuples<list<SubRegIndex> Indices, RegisterClass RC,
                    int last_reg, int stride, int size, string prefix>
  : RegisterTuples<Indices, 
                   RegSeqDags<RC, last_reg, stride, size>.ret,
                   RegSeqNames<last_reg, stride, size, prefix>.ret>;


// Register Classes
def VReg1  : GASSRegClass<[i1], 1, (add (sequence "PR%u", 0, 7), 
                                        PT, NPT)>; 
def VReg32 : GASSRegClass<[i32,f32,v2f16], 32, (add (sequence "VGPR%u", 0, 255), 
                                                    RZ32)>;

// def : RegisterWithSubRegs<"", 
//                           [VREG0, VREG1], [VREG2, VREG3], ...>;
def VReg64Tuple : GASSRegTuples<[sub0, sub1], VReg32, 255, 2, 2, "R">;
def VReg64 : GASSRegClass<[i64, f64, v2f32, v2i32, v4f16], 
                          64, (add VReg64Tuple, RZ64)>;
def VReg128Tuple : GASSRegTuples<[sub0, sub1, sub2, sub3], 
                                 VReg32, 255, 4, 4, "R">;
def VReg128 : GASSRegClass<[v2i64, v2f64, v4i32, v4f32, 
                            v8f16, v8i16, i128, f128], 128, 
                           (add VReg128Tuple)>;