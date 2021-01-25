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
  def SGPR#Index : GASSReg<"s"#Index, Index>;
}

foreach Index = 0-256 in {
  def VGPR#Index : GASSReg<"v"#Index, Index>;
}

foreach Index = 0-7 in {
  def PR#Index : GASSReg<"p"#Index, Index>;
}

// Zero registers
def PT   : GASSReg<"PT", 0x7>;
def NPT  : GASSReg<"!PT", 0xf>;
def RZ32 : GASSReg<"RZ", 0xff>;
def RZ64 : GASSReg<"RZ", 0xff>;

// Vector registers
// Following the practice in AMDGPU (SIRegisterInfo.td)
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
                   RegSeqDags<RC, last_reg, stride, size>.ret>;


// Register Classes
def VReg1  : GASSRegClass<[i1], /*TODO: check this*/1, (add (sequence "PR%u", 0, 7))>; 
def VReg32 : GASSRegClass<[i32,f32,v2f16], 32, (add (sequence "VGPR%u", 0, 255))>;

// def : RegisterWithSubRegs<"", 
//                           [VREG0, VREG1], [VREG2, VREG3], ...>;
def VReg64Tuple : GASSRegTuples<[sub0, sub1], VReg32, 255, 2, 2, "v">;

def VReg64 : GASSRegClass<[i64, f64], 64, (add VReg64Tuple)>;