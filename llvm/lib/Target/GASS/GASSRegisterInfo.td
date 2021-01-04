class GASSReg<string n, bits<16> enc> : Register<n> {
  let Namespace = "GASS";
  let HWEncoding = enc;
}

class GASSRegClass<list<ValueType> regTypes, int alignment, dag regList>
  : RegisterClass<"GASS", regTypes, alignment, regList>;

foreach Index = 0-64 in {
  def SGPR#Index : GASSReg<"s"#Index, Index>;
}

foreach Index = 0-256 in {
  def VGPR#Index : GASSReg<"v"#Index, Index>;
}

// Register Classes
def VReg32 : GASSRegClass<[i32,f32,v2f16], 32, (add (sequence "VGPR%u", 0, 255))>;