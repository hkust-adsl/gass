#include "GASSMCTargetDesc.h"
#include "GASSELFStreamer.h"
#include "NvInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/MC/MCSubtargetInfo.h"

using namespace llvm;

GASSTargetELFStreamer::GASSTargetELFStreamer(MCStreamer &S,
                                             const MCSubtargetInfo &STI)
  : GASSTargetStreamer(S), STI(STI) {}

MCELFStreamer &GASSTargetELFStreamer::getStreamer() {
  return static_cast<MCELFStreamer &>(Streamer);
}

// Override virtual functions
void GASSTargetELFStreamer::emitAttributes(unsigned SmVersion) {
  MCAssembler &MCA = getStreamer().getAssembler();

  unsigned EFlags = MCA.getELFHeaderEFlags();
  
  EFlags |= (SmVersion);
  EFlags |= (SmVersion) << 16;

  MCA.setELFHeaderEFlags(EFlags);
}

/// emit .nv.info (per module)
void GASSTargetELFStreamer::emitNvInfo(
    std::vector<std::unique_ptr<NvInfo>> &Info) {
  unsigned Size = 0;
  std::vector<char> Buf;
  for (auto &I : Info) {
    std::vector<char> LocalBuf = I->getEncoding(this);
    Buf.insert(Buf.end(), LocalBuf.begin(), LocalBuf.end());
    Size += LocalBuf.size();
  }
  getStreamer().emitBytes(StringRef(Buf.data(), Size));
}

/// emit .nv.info.{name} (per function)
void GASSTargetELFStreamer::emitNvInfoFunc(
    std::vector<std::unique_ptr<NvInfo>> &Info) {
  unsigned Size = 0;
  std::vector<char> Buf;
  for (auto &I : Info) {
    std::vector<char> LocalBuf = I->getEncoding(this);
    Buf.insert(Buf.end(), LocalBuf.begin(), LocalBuf.end());
    Size += LocalBuf.size();
  }
  getStreamer().emitBytes(StringRef(Buf.data(), Size));
}

/// predicate symtab index of machinefunction
unsigned GASSTargetELFStreamer::predicateSymtabIndex(MachineFunction *MF, 
                                                     char Modifier) const {
  // order of symtab: (for each machine function)
  // .text.{name}
  // .nv.constant0.{name}
  
  // MF0
  // MF1 
  // ...
  auto Iter = std::find(MFs.begin(), MFs.end(), MF);
  assert(Iter != MFs.end() && "Should have visited this MF");
  unsigned MFIdx = std::distance(MFs.begin(), Iter);

  if (Modifier == 'c') // .nv.constant0.{name}
    return 1 + 2*MFs.size() + 1 + MFIdx;
  
  return 1 + 2*NumMFs + MFIdx;
}