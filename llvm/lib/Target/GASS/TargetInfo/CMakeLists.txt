add_llvm_component_library(LLVMGASSInfo
  GASSTargetInfo.cpp

  LINK_COMPONENTS
  Support

  ADD_TO_COMPONENT
  GASS
)