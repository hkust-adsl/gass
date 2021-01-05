add_llvm_component_group(GASS)

set(LLVM_TARGET_DEFINITIONS GASS.td)

tablegen(LLVM GASSGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM GASSGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM GASSGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM GASSGenSubtargetInfo.inc -gen-subtarget)
tablegen(LLVM GASSGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM GASSGenDAGISel.inc -gen-dag-isel)
# combiner

add_public_tablegen_target(GASSCommonTableGen)

# TODO: MCCodeEmitter.inc

add_llvm_target(GASSCodeGen
  GASSAsmPrinter.cpp
  GASSFrameLowering.cpp
  GASSInstrInfo.cpp
  GASSISelDAGToDAG.cpp
  GASSISelLowering.cpp
  GASSMCInstLowering.cpp
  GASSRegisterInfo.cpp
  GASSSubtarget.cpp
  GASSTargetMachine.cpp

  LINK_COMPONENTS
  AsmPrinter
  GASSInfo
  GASSDesc
  SelectionDAG
  Support
  Vectorize

  ADD_TO_COMPONENT
  GASS
)

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)