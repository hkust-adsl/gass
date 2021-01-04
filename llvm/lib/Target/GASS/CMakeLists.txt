add_llvm_component_group(GASS)

set(LLVM_TARGET_DEFINITIONS GASS.td)

tablegen(LLVM GASSGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM GASSGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM GASSGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM GASSGenSubtargetInfo.inc -gen-subtarget)
tablegen(LLVM GASSGenMCCodeEmitter.inc -gen-emitter)

tablegen(LLVM GASSGenGlobalISel.inc -gen-global-isel)
tablegen(LLVM GASSGenRegisterBank.inc -gen-register-bank)
# combiner

add_public_tablegen_target(GASSCommonTableGen)

# TODO: MCCodeEmitter.inc

add_llvm_target(GASSCodeGen
  GASSFrameLowering.cpp
  GASSInstructionSelector.cpp
  GASSInstrInfo.cpp
  GASSISelLowering.cpp
  GASSRegisterInfo.cpp
  GASSRegisterBankInfo.cpp
  GASSSubtarget.cpp
  GASSTargetMachine.cpp

  LINK_COMPONENTS
  GASSInfo
  GASSDesc
  GlobalISel
  Support
  Vectorize

  ADD_TO_COMPONENT
  GASS
)

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)