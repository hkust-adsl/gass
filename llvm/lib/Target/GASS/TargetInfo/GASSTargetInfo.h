#ifndef LLVM_LIB_TARGET_GASS_TARGETINFO_H
#define LLVM_LIB_TARGET_GASS_TARGETINFO_H

namespace llvm {
class Target;

Target &getTheGASSTarget();
}

#endif