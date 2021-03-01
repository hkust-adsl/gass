#ifndef LLVM_LIB_TARGET_GASS_MCTARGETDESC_NVINFO_H
#define LLVM_LIB_TARGET_GASS_MCTARGETDESC_NVINFO_H

#include "GASSELFStreamer.h"
#include "GASSTargetStreamer.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/Support/raw_ostream.h"

#include <vector>
#include <string>

namespace llvm {
class StringRef;
class MachineFunction;
class GASSTargetELFStreamer;

enum NvInfoAttrType : unsigned {
  EIATTR_REGCOUNT,
  EIATTR_MAX_STACK_SIZE,
  EIATTR_MIN_STACK_SIZE,
  EIATTR_FRAME_SIZE,
  EIATTR_SW_WAR,
  EIATTR_CUDA_API_VERSION,
  EIATTR_PARAM_CBANK,
  EIATTR_CBANK_PARAM_SIZE,
  EIATTR_CBANK_KPARAM_INFO,
  EIATTR_MAXREG_COUNT,
  EIATTR_INT_WARP_WIDE_INSTR_OFFSETS,
  EIATTR_COOP_GROUP_INSTR_OFFSETS,
  EIATTR_EXIT_INSTR_OFFSETS,
};

class NvInfo {
  // Type
  NvInfoAttrType InfoType;

  // Encoding for each type (nvdisasm treat it as 2 bytes not short)
  char EntryEncoding[2];

  /// TODO: Size of this Entry in bytes 
  short EntrySize = 0;

  /// MachineFunction this attr points to
  MachineFunction *MF = nullptr;

  /// Symtab Idx of corresponding MF
  unsigned SymtabIdx = 0;

  /// Store value for REGCOUNT, STACK_SIZE, FRAME_SIZE, CUDA_API_VERSION,
  /// SW_WAR, 
  unsigned Value = 0;

  /// Store value for PARAM_CBANK
  short ParamOffset;
  short ParamSize;

  /// ParamInfo
  short KParamOrdinal;
  short KParamOffset;
  char KParamSize;
  char KParamLogAlign;

  std::vector<unsigned> EXITOffsets;

  short MaxRegCount;
public:
  NvInfo(NvInfoAttrType NVINFOType);

  /// To binary encoding (emit byte to stream)
  std::vector<char> getEncoding(const GASSTargetELFStreamer * GTS) const;

  /// To asm (emit to OS)
  std::string getStr() const;

  StringRef getAttrName() const;

  /// Modifiers
  void setRegCount(unsigned Count, MachineFunction *MF) {
    Value = Count;
    this->MF = MF;
    EntrySize += /* symtb idx */ 4 + sizeof(Count);
  }
  void setMaxStackSize(unsigned MaxSize, MachineFunction *MF) {
    Value = MaxSize;
    this->MF = MF;
    EntrySize += /* symtb idx */ 4 + sizeof(MaxSize);
  }
  void setMinStackSize(unsigned MinSize, MachineFunction *MF) {
    Value = MinSize;
    this->MF = MF;
    EntrySize += /* symtb idx */ 4 + sizeof(MinSize);
  }
  void setFrameSize(unsigned FrameSize, MachineFunction *MF) {
    Value = FrameSize;
    this->MF = MF;
    EntrySize += /* symtb idx */ 4 + sizeof(FrameSize);
  }

  // per function
  void setSwWar(bool SwWar) { 
    Value = SwWar; 
    EntrySize += 4;
  }
  void setCudaVersion(unsigned CudaVersion) { 
    Value = CudaVersion; 
    EntrySize += sizeof(CudaVersion);
  }
  void setParamCBank(short Offset, short Size, MachineFunction *MF) { 
    ParamOffset = Offset;
    ParamSize = Size;
    this->MF = MF;
    EntrySize += /* symtb idx */ 4 + sizeof(ParamOffset) + sizeof(ParamSize);
  }
  void setCBankParamSize(short Size) { 
    ParamSize = Size; 
    EntrySize += sizeof(Size);
  }
  void setKParamInfo(unsigned Idx, unsigned Ordinal, unsigned Offset,
                     unsigned Size, unsigned LogAlign) { 
    KParamOrdinal = Ordinal;
    KParamOffset = Offset;
    KParamLogAlign = LogAlign;
    KParamSize = Size;
    EntrySize += 12; 
  }
  void setMaxRegCount(short MaxCount) { 
    MaxRegCount = MaxCount; 
    EntrySize += sizeof(MaxCount);
  }
  void setEXITOffsets(std::vector<unsigned> Offsets) {
    EXITOffsets = Offsets;
    EntrySize += Offsets.size() * 4;
  }

  /// Observers
};

/// helper
// per module
NvInfo *createNvInfoRegCount(unsigned Count, MachineFunction *MF);
NvInfo *createNvInfoMaxStackSize(unsigned MaxSize, MachineFunction *MF);
NvInfo *createNvInfoMinStackSize(unsigned MinSize, MachineFunction *MF);
NvInfo *createNvInfoFrameSize(unsigned FrameSize, MachineFunction *MF);
// per function
NvInfo *createNvInfoSwWar(bool SwWar);
NvInfo *createNvInfoCudaVersion(unsigned CudaVersion);
NvInfo *createNvInfoParamCBank(short Offset, short Size, MachineFunction *MF);
NvInfo *createNvInfoCBankParamSize(unsigned Size);
NvInfo *createNvInfoKParamInfo(unsigned Idx, unsigned Ordinal, unsigned Offset,
                              unsigned Size, unsigned LogAlign);
NvInfo *createNvInfoMaxRegCount(short MaxCount);
NvInfo *createNvInfoExitInstrOffsets(std::vector<unsigned> Offsets);

} // namespace llvm

#endif