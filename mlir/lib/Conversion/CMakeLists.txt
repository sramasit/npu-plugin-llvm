add_subdirectory(AffineToStandard)
add_subdirectory(AMDGPUToROCDL)
add_subdirectory(ArithCommon)
add_subdirectory(ArithToAMDGPU)
add_subdirectory(ArithToArmSME)
add_subdirectory(ArithToLLVM)
if(MLIR_DIALECT_SPIRV_ENABLE)
  add_subdirectory(ArithToSPIRV)
endif()
add_subdirectory(ArmNeon2dToIntr)
add_subdirectory(ArmSMEToSCF)
add_subdirectory(ArmSMEToLLVM)
add_subdirectory(AsyncToLLVM)
add_subdirectory(BufferizationToMemRef)
add_subdirectory(ComplexToLibm)
add_subdirectory(ComplexToLLVM)
add_subdirectory(ComplexToSPIRV)
add_subdirectory(ComplexToStandard)
add_subdirectory(ControlFlowToLLVM)
add_subdirectory(ControlFlowToSCF)
add_subdirectory(ConvertToLLVM)
if(MLIR_DIALECT_SPIRV_ENABLE)
  add_subdirectory(ControlFlowToSPIRV)
endif()
add_subdirectory(FuncToLLVM)
if(MLIR_DIALECT_SPIRV_ENABLE)
  add_subdirectory(FuncToSPIRV)
endif()
add_subdirectory(GPUCommon)
add_subdirectory(GPUToNVVM)
add_subdirectory(GPUToROCDL)
if(MLIR_DIALECT_SPIRV_ENABLE)
  add_subdirectory(GPUToSPIRV)
endif()
add_subdirectory(GPUToVulkan)
add_subdirectory(IndexToLLVM)
add_subdirectory(IndexToSPIRV)
add_subdirectory(LinalgToStandard)
add_subdirectory(LLVMCommon)
add_subdirectory(MathToFuncs)
add_subdirectory(MathToLibm)
add_subdirectory(MathToLLVM)
if(MLIR_DIALECT_SPIRV_ENABLE)
  add_subdirectory(MathToSPIRV)
endif()
add_subdirectory(MemRefToLLVM)
if(MLIR_DIALECT_SPIRV_ENABLE)
  add_subdirectory(MemRefToSPIRV)
endif()
add_subdirectory(NVGPUToNVVM)
add_subdirectory(NVVMToLLVM)
add_subdirectory(OpenACCToSCF)
add_subdirectory(OpenMPToLLVM)
add_subdirectory(PDLToPDLInterp)
add_subdirectory(ReconcileUnrealizedCasts)
add_subdirectory(SCFToControlFlow)
add_subdirectory(SCFToEmitC)
add_subdirectory(SCFToGPU)
add_subdirectory(SCFToOpenMP)
if(MLIR_DIALECT_SPIRV_ENABLE)
  add_subdirectory(SCFToSPIRV)
endif()
add_subdirectory(ShapeToStandard)
if(MLIR_DIALECT_SPIRV_ENABLE)
  add_subdirectory(SPIRVToLLVM)
endif()
add_subdirectory(TensorToLinalg)
if(MLIR_DIALECT_SPIRV_ENABLE)
  add_subdirectory(TensorToSPIRV)
endif()
add_subdirectory(TosaToArith)
add_subdirectory(TosaToLinalg)
add_subdirectory(TosaToMLProgram)
add_subdirectory(TosaToSCF)
add_subdirectory(TosaToTensor)
add_subdirectory(UBToLLVM)
add_subdirectory(UBToSPIRV)
add_subdirectory(VectorToArmSME)
add_subdirectory(VectorToGPU)
add_subdirectory(VectorToLLVM)
add_subdirectory(VectorToSCF)
if(MLIR_DIALECT_SPIRV_ENABLE)
  add_subdirectory(VectorToSPIRV)
endif()
