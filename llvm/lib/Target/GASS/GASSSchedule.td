// 3. SchedWrite(s) (& SchedRead(s))?
def WriteINT : SchedWrite; 
def WriteSHIFT : SchedWrite;
def WriteLOP3 : SchedWrite;
def WriteISETP : SchedWrite;

// Integer mulitplication
def WriteIMUL : SchedWrite;
def WriteIMULWIDE : SchedWrite;

// Special units
def WriteF2F : SchedWrite;

// Floating points
def WriteFP32 : SchedWrite;
def WriteFP64 : SchedWrite;

// Tensor Cores
def WriteHMMA884 : SchedWrite;

// LDST
def WriteLDG : SchedWrite;
def WriteSTG : SchedWrite;
def WriteLDS : SchedWrite;
def WriteSTS : SchedWrite;
