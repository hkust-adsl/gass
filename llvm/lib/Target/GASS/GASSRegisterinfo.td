class GASSReg<string n, bits<8> enc> : Register<n> {
  let Namespace = "GASS";
  let HWEncoding{7-0} = enc;
}

foreach Index = 0...256 in {
  def VGPR : ;
}