add_llvm_component_library(LLVMGASSDesc
  GASSInstPrinter.cpp
  GASSMCAsmInfo.cpp
  GASSMCCodeEmitter.cpp
  GASSMCTargetDesc.cpp
  GASSAsmStreamer.cpp
  GASSAsmBackend.cpp
  GASSELFStreamer.cpp
  GASSTargetStreamer.cpp
  
  LINK_COMPONENTS
  MC
  GASSInfo
  Support

  ADD_TO_COMPONENT
  GASS
)