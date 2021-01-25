// 3. SchedWrite(s) (& SchedRead(s))?
def WriteINT : SchedWrite; 

// Floating points
def WriteFP32 : SchedWrite;
def WriteFP64 : SchedWrite;

// LDST
def WriteLD : SchedWrite;
def WriteST : SchedWrite;
