#ifndef LLVM_LIB_TARGET_GASS_GASSTARGETMACHINE_H
#define LLVM_LIB_TARGET_GASS_GASSTARGETMACHINE_H

#include "llvm/Target/TargetMachine.h"

#include <memory>

namespace llvm {

class GASSTargetMachine : public LLVMTargetMachine {
private:
  std::unique_ptr<GASSSubtarget> Subtarget;
  std::unique_ptr<TargetLoweringObjectFile> TLOF;

public:
  GASSTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                    StringRef FS, TargetOption Options, 
                    Optional<Reloc::Model> RM, Optional<CodeModel::Model> CM,
                    CodeGenOpt::Level OL, bool JIT);
  
  ~GASSTargetMachine() override;

  const GASSSubtarget *getSubtargetImpl (const Function &) const override {
    return Subtarget.get();
  }
  TargetLoweringObjectFile *getObjFileLowering() const override { return TLOF.get(); }
  TargetTransformInfo getTargetTransformInfo(const Function &F) override;

  void adjustPassManager(PassManagerBuilder &) override;
};

} // namespace llvm

#endif // LLVM_LIB_TARGET_GASS_GASSTARGETMACHINE_H