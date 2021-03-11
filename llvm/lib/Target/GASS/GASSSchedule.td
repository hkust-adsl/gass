// 3. SchedWrite(s) (& SchedRead(s))?
def WriteINT : SchedWrite; 
def WriteISETP : SchedWrite;

// Floating points
def WriteFP32 : SchedWrite;
def WriteFP64 : SchedWrite;

// Tensor Cores
def WriteTC : SchedWrite;

// LDST
def WriteLD : SchedWrite;
def WriteST : SchedWrite;
