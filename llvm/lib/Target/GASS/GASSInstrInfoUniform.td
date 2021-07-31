multiclass ULDC<RegisterClass RC, int Width> {
  def c : GASSPseudoInst<(outs RC:$dst), (ins i32const:$offset), []>;
  // def rc : ;
}

defm ULDC32 : ULDC<SReg32, 32>, Sched<[WriteINT]>, Requires<[hasSM75]>;
defm ULDC64 : ULDC<SReg64, 64>, Sched<[WriteINT]>, Requires<[hasSM75]>;

// def : Pat<(i32 (ldc (i32 timm:$offset))), (ULDC32c i32const:$offset)>;
// def : Pat<(i64 (ldc (i32 timm:$offset))), (ULDC64c i32const:$offset)>;

// TODO:
//   UISETP, UIADD3, UMOV, USHF, UIMAD
// Other:
//   ULEA, ULOP3, 
// ISel: (VReg + UReg)
//   LDSM, 
//   STS,  (also for Turing?)
//   LDGSTS