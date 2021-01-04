add_llvm_component_library(LLVMGASSDesc
  GASSMCTargetDesc.cpp
  
  LINK_COMPONENTS
  MC
  GASSInfo
  Support

  ADD_TO_COMPONENT
  GASS
)