#include "NvInfo.h"
#include "GASSTargetStreamer.h"
#include "GASSELFStreamer.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

/// NvInfo class member functions
NvInfo::NvInfo(NvInfoAttrType NVINFOType) 
    : InfoType(NVINFOType){
  switch (NVINFOType) {
  default: llvm_unreachable("Invalid .nv.info type");
  case EIATTR_REGCOUNT:
    EntryEncoding[0] = 0x4;
    EntryEncoding[1] = 0x2f;
    break;
  case EIATTR_MAX_STACK_SIZE:
    EntryEncoding[0] = 0x4;
    EntryEncoding[1] = 0x23;
    break;
  case EIATTR_MIN_STACK_SIZE:
    EntryEncoding[0] = 0x4;
    EntryEncoding[1] = 0x12;
    break;
  case EIATTR_FRAME_SIZE:
    EntryEncoding[0] = 0x4;
    EntryEncoding[1] = 0x11;
    break;
  case EIATTR_SW_WAR:
    EntryEncoding[0] = 0x4;
    EntryEncoding[1] = 0x36;
    break;
  case EIATTR_CUDA_API_VERSION:
    EntryEncoding[0] = 0x4;
    EntryEncoding[1] = 0x37;
    break;
  case EIATTR_PARAM_CBANK:
    EntryEncoding[0] = 0x4;
    EntryEncoding[1] = 0x0a;
    break;
  case EIATTR_CBANK_PARAM_SIZE:
    EntryEncoding[0] = 0x3;
    EntryEncoding[1] = 0x19;
    break;
  case EIATTR_CBANK_KPARAM_INFO:
    EntryEncoding[0] = 0x4;
    EntryEncoding[1] = 0x17;
    break;
  case EIATTR_MAXREG_COUNT:
    EntryEncoding[0] = 0x3;
    EntryEncoding[1] = 0x1b;
    break;
  case EIATTR_INT_WARP_WIDE_INSTR_OFFSETS:
    EntryEncoding[0] = 0x4;
    EntryEncoding[1] = 0x31;
    break;
  case EIATTR_COOP_GROUP_INSTR_OFFSETS:
    EntryEncoding[0] = 0x4;
    EntryEncoding[1] = 0x28;
    break;
  case EIATTR_EXIT_INSTR_OFFSETS:
    EntryEncoding[0] = 0x4;
    EntryEncoding[1] = 0x1c;
    break;
  }
}

StringRef NvInfo::getAttrName() const {
  switch (InfoType) {
  default: llvm_unreachable("Invalid .nv.info type");
  case EIATTR_REGCOUNT:
    return "EIATTR_REGCOUNT";
  case EIATTR_MAX_STACK_SIZE:
    return "EIATTR_MAX_STACK_SIZE";
  case EIATTR_MIN_STACK_SIZE:
    return "EIATTR_MIN_STACK_SIZE";
  case EIATTR_FRAME_SIZE:
    return "EIATTR_FRAME_SIZE";
  case EIATTR_SW_WAR:
    return "EIATTR_SW_WAR";
  case EIATTR_CUDA_API_VERSION:
    return "EIATTR_CUDA_API_VERSION";
  case EIATTR_PARAM_CBANK:
    return "EIATTR_PARAM_CBANK";
  case EIATTR_CBANK_PARAM_SIZE:
    return "EIATTR_CBANK_PARAM_SIZE";
  case EIATTR_CBANK_KPARAM_INFO:
    return "EIATTR_CBANK_KPARAM_INFO";
  case EIATTR_MAXREG_COUNT:
    return "EIATTR_MAXREG_COUNT";
  case EIATTR_INT_WARP_WIDE_INSTR_OFFSETS:
    return "EIATTR_INT_WARP_WIDE_INSTR_OFFSETS";
  case EIATTR_COOP_GROUP_INSTR_OFFSETS:
    return "EIATTR_COOP_GROUP_INSTR_OFFSETS";
  case EIATTR_EXIT_INSTR_OFFSETS:
    return "EIATTR_EXIT_INSTR_OFFSETS";
  }
}

/// get data to emit to stream
std::vector<char> NvInfo::getEncoding(const GASSTargetELFStreamer *GTS) const {
  // query MF symtab idx (if exist)
  unsigned SymtabIdx = 0;
  if (MF) 
    SymtabIdx = GTS->predicateSymtabIndex(MF);

  std::vector<char> Enc;
  // Entry Encoding
  Enc.push_back(EntryEncoding[0]);
  Enc.push_back(EntryEncoding[1]);
  // Size Encoding (EIATTR_CBANK_PARAM_SIZE doesn't have)
  if (InfoType != EIATTR_CBANK_PARAM_SIZE &&
      InfoType != EIATTR_MAXREG_COUNT) {
    char EntrySizeBuf[2];
    memcpy(EntrySizeBuf, &EntrySize, sizeof(EntrySize));
    Enc.insert(Enc.end(), EntrySizeBuf, EntrySizeBuf + sizeof(EntrySize));
  }

  // Buffers
  char SymIdxBuf[4];
  char ValueBuf[4];

  switch (InfoType) {
  default: llvm_unreachable("Invalid IntoType");
  case EIATTR_REGCOUNT: 
  case EIATTR_MAX_STACK_SIZE:
  case EIATTR_MIN_STACK_SIZE:
  case EIATTR_FRAME_SIZE:
    // symbol index    
    memcpy(SymIdxBuf, &SymtabIdx, sizeof(SymtabIdx));
    Enc.insert(Enc.end(), SymIdxBuf, SymIdxBuf + sizeof(SymtabIdx));
    // Value
    memcpy(ValueBuf, &Value, sizeof(Value));
    Enc.insert(Enc.end(), ValueBuf, ValueBuf + sizeof(Value));
    break;
  // per function
  case EIATTR_SW_WAR:
  case EIATTR_CUDA_API_VERSION:
    memcpy(ValueBuf, &Value, sizeof(Value));
    Enc.insert(Enc.end(), ValueBuf, ValueBuf + sizeof(Value));
    break;
  case EIATTR_PARAM_CBANK:
    SymtabIdx = GTS->predicateSymtabIndex(MF, 'c');
    memcpy(SymIdxBuf, &SymtabIdx, sizeof(SymtabIdx));
    Enc.insert(Enc.end(), SymIdxBuf, SymIdxBuf + sizeof(SymtabIdx));
    memcpy(ValueBuf, &ParamOffset, sizeof(ParamOffset));
    Enc.insert(Enc.end(), ValueBuf, ValueBuf + sizeof(ParamOffset));
    memcpy(ValueBuf, &ParamSize, sizeof(ParamSize));
    Enc.insert(Enc.end(), ValueBuf, ValueBuf + sizeof(ParamSize));
    break;
  case EIATTR_CBANK_PARAM_SIZE:
    // short
    memcpy(ValueBuf, &ParamSize, sizeof(ParamSize));
    Enc.insert(Enc.end(), ValueBuf, ValueBuf + sizeof(ParamSize));
    break;
  case EIATTR_MAXREG_COUNT:
    memcpy(ValueBuf, &MaxRegCount, sizeof(MaxRegCount));
    Enc.insert(Enc.end(), ValueBuf, ValueBuf + sizeof(MaxRegCount));
    break;
  case EIATTR_CBANK_KPARAM_INFO: {
    memcpy(ValueBuf, &Value, sizeof(Value)); // 0
    Enc.insert(Enc.end(), ValueBuf, ValueBuf + sizeof(Value));
    memcpy(ValueBuf, &KParamOrdinal, sizeof(KParamOrdinal));
    Enc.insert(Enc.end(), ValueBuf, ValueBuf + sizeof(KParamOrdinal));
    memcpy(ValueBuf, &KParamOffset, sizeof(KParamOffset));
    Enc.insert(Enc.end(), ValueBuf, ValueBuf + sizeof(KParamOffset));
    // FIXME: this is wrong.
    unsigned V = 0x1f000 + unsigned((KParamSize / 4) << 20); // FIXME FIXME
    memcpy(ValueBuf, &V, sizeof(V));
    Enc.insert(Enc.end(), ValueBuf, ValueBuf + sizeof(V));
    break;
  }
  case EIATTR_INT_WARP_WIDE_INSTR_OFFSETS:
    break;
  case EIATTR_COOP_GROUP_INSTR_OFFSETS:
    llvm_unreachable("Not implemented");
    break;
  case EIATTR_EXIT_INSTR_OFFSETS:
    for (int i = 0; i < EXITOffsets.size(); ++i) {
      memcpy(ValueBuf, &EXITOffsets[i], sizeof(unsigned));
      Enc.insert(Enc.end(), ValueBuf, ValueBuf + sizeof(unsigned));
    }
    break;  
  }

  return Enc;
}

/// get asm to emit to stream
std::string NvInfo::getStr() const {
  // TODO: fill this.
  return "";
}


/// Public creators
NvInfo *llvm::createNvInfoRegCount(unsigned Count, MachineFunction *MF) {
  NvInfo *NI = new NvInfo(EIATTR_REGCOUNT);
  NI->setRegCount(Count, MF);
  return NI;
}

NvInfo *llvm::createNvInfoMaxStackSize(unsigned MaxSize, MachineFunction *MF) {
  NvInfo *NI = new NvInfo(EIATTR_MAX_STACK_SIZE);
  NI->setMaxStackSize(MaxSize, MF);
  return NI;
}

NvInfo *llvm::createNvInfoMinStackSize(unsigned MinSize, MachineFunction *MF) {
  NvInfo *NI = new NvInfo(EIATTR_MIN_STACK_SIZE);
  NI->setMinStackSize(MinSize, MF);
  return NI;
}

NvInfo *llvm::createNvInfoFrameSize(unsigned FrameSize, MachineFunction *MF) {
  NvInfo *NI = new NvInfo(EIATTR_FRAME_SIZE);
  NI->setFrameSize(FrameSize, MF);
  return NI;
}

NvInfo *llvm::createNvInfoSwWar(bool SwWar) {
  NvInfo *NI = new NvInfo(EIATTR_SW_WAR);
  NI->setSwWar(SwWar);
  return NI;
}

NvInfo *llvm::createNvInfoCudaVersion(unsigned CudaVersion) {
  NvInfo *NI = new NvInfo(EIATTR_CUDA_API_VERSION);
  NI->setCudaVersion(CudaVersion);
  return NI;
}

NvInfo *llvm::createNvInfoParamCBank(short ParamOffset, short Size, 
                                     MachineFunction *MF) {
  NvInfo *NI = new NvInfo(EIATTR_PARAM_CBANK);
  NI->setParamCBank(ParamOffset, Size, MF);
  return NI;
}

NvInfo *llvm::createNvInfoCBankParamSize(unsigned Size) {
  NvInfo *NI = new NvInfo(EIATTR_CBANK_PARAM_SIZE);
  NI->setCBankParamSize(Size);
  return NI;
}

NvInfo *llvm::createNvInfoKParamInfo(unsigned Idx, unsigned Ordinal, 
                                     unsigned Offset,
                                     unsigned Size, unsigned LogAlign) {
  NvInfo *NI = new NvInfo(EIATTR_CBANK_KPARAM_INFO);
  NI->setKParamInfo(Idx, Ordinal, Offset, Size, LogAlign);
  return NI;
}

NvInfo *llvm::createNvInfoMaxRegCount(short MaxCount) {
  NvInfo *NI = new NvInfo(EIATTR_MAXREG_COUNT);
  NI->setMaxRegCount(MaxCount);
  return NI;
}

NvInfo *llvm::createNvInfoExitInstrOffsets(std::vector<unsigned> Offsets) {
  NvInfo *NI = new NvInfo(EIATTR_EXIT_INSTR_OFFSETS);
  NI->setEXITOffsets(Offsets);
  return NI;
}