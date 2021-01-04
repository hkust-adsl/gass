#include "GASSMCAsmInfo.h"
#include "llvm/ADT/Triple.h"

using namespace llvm;

GASSMCAsmInfo::GASSMCAsmInfo(const Triple &TheTriple, 
                             const MCTargetOptions &Options) {
  CommentString = "//";

  // Affect how the output asm file looks like.
  HasSingleParameterDotFile  = false;
  HasDotTypeDotSizeDirective = false;
  SupportsQuotedNames = false;
  SupportsExtendedDwarfLocDirective = false;
  SupportsSignedData = false;

  AsciiDirective = nullptr; // not supported
  AscizDirective = nullptr; // not supported

  WeakDirective   = "\t// .weak\t";
  GlobalDirective = "\t// .globl\t";

  UseIntegratedAssembler = false;
}