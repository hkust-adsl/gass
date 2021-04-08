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

add_llvm_target(GASSCodeGen
  GASSAddAddrSpace.cpp
  GASSAsmPrinter.cpp
  GASSBarrierSettingPass.cpp
  GASSBranchOffsetPass.cpp
  GASSCodeGenPrepare.cpp
  GASSCodeSink.cpp
  GASSExpandPreRAPseudo.cpp
  GASSFrameLowering.cpp
  GASSIfConversion.cpp
  GASSInstrInfo.cpp
  GASSISelDAGToDAG.cpp
  GASSISelLowering.cpp
  GASSMachineFunctionCFGPrinter.cpp
  GASSMCInstLowering.cpp
  GASSPreEmitPreparePass.cpp
  GASSRegisterInfo.cpp
  GASSRegPressureCompute.cpp
  GASSSchedStrategy.cpp
  GASSStallSettingPass.cpp
  GASSSubtarget.cpp
  GASSTargetMachine.cpp
  GASSTargetObjectFile.cpp
  LiveBarRange.cpp

  LINK_COMPONENTS
  AsmPrinter
  GASSInfo
  GASSDesc
  SelectionDAG
  Support
  Vectorize
  TransformUtils

  ADD_TO_COMPONENT
  GASS
)

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)