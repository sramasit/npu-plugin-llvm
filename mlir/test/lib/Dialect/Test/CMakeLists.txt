set(LLVM_OPTIONAL_SOURCES
  TestDialect.cpp
  TestPatterns.cpp
  TestTraits.cpp
  TestToLLVMIRTranslation.cpp
)

set(LLVM_TARGET_DEFINITIONS TestInterfaces.td)
mlir_tablegen(TestAttrInterfaces.h.inc -gen-attr-interface-decls)
mlir_tablegen(TestAttrInterfaces.cpp.inc -gen-attr-interface-defs)
mlir_tablegen(TestTypeInterfaces.h.inc -gen-type-interface-decls)
mlir_tablegen(TestTypeInterfaces.cpp.inc -gen-type-interface-defs)
mlir_tablegen(TestOpInterfaces.h.inc -gen-op-interface-decls)
mlir_tablegen(TestOpInterfaces.cpp.inc -gen-op-interface-defs)
add_public_tablegen_target(MLIRTestInterfaceIncGen)

set(LLVM_TARGET_DEFINITIONS TestOps.td)
mlir_tablegen(TestAttrDefs.h.inc -gen-attrdef-decls)
mlir_tablegen(TestAttrDefs.cpp.inc -gen-attrdef-defs)
add_public_tablegen_target(MLIRTestAttrDefIncGen)

set(LLVM_TARGET_DEFINITIONS TestTypeDefs.td)
mlir_tablegen(TestTypeDefs.h.inc -gen-typedef-decls -typedefs-dialect=test)
mlir_tablegen(TestTypeDefs.cpp.inc -gen-typedef-defs -typedefs-dialect=test)
add_public_tablegen_target(MLIRTestTypeDefIncGen)

set(LLVM_TARGET_DEFINITIONS TestEnumDefs.td)
mlir_tablegen(TestOpEnums.h.inc -gen-enum-decls)
mlir_tablegen(TestOpEnums.cpp.inc -gen-enum-defs)
add_public_tablegen_target(MLIRTestEnumDefIncGen)

set(LLVM_TARGET_DEFINITIONS TestOps.td)
mlir_tablegen(TestOps.h.inc -gen-op-decls)
mlir_tablegen(TestOps.cpp.inc -gen-op-defs)
mlir_tablegen(TestOpsDialect.h.inc -gen-dialect-decls -dialect=test)
mlir_tablegen(TestOpsDialect.cpp.inc -gen-dialect-defs -dialect=test)
mlir_tablegen(TestPatterns.inc -gen-rewriters)
add_public_tablegen_target(MLIRTestOpsIncGen)

set(LLVM_TARGET_DEFINITIONS TestOpsSyntax.td)
mlir_tablegen(TestOpsSyntax.h.inc -gen-op-decls)
mlir_tablegen(TestOpsSyntax.cpp.inc -gen-op-defs)
add_public_tablegen_target(MLIRTestOpsSyntaxIncGen)

set(LLVM_TARGET_DEFINITIONS Passes.td)
mlir_tablegen(Passes.h.inc -gen-pass-decls -name Test)
add_public_tablegen_target(MLIRTestPassIncGen)

# Exclude tests from libMLIR.so
add_mlir_library(MLIRTestDialect
  TestAttributes.cpp
  TestDialect.cpp
  TestInterfaces.cpp
  TestPatterns.cpp
  TestTraits.cpp
  TestTypes.cpp
  TestOpsSyntax.cpp
  TestDialectInterfaces.cpp
  Passes.cpp

  EXCLUDE_FROM_LIBMLIR

  DEPENDS
  MLIRTestAttrDefIncGen
  MLIRTestEnumDefIncGen
  MLIRTestInterfaceIncGen
  MLIRTestTypeDefIncGen
  MLIRTestOpsIncGen
  MLIRTestOpsSyntaxIncGen
  MLIRTestPassIncGen

  LINK_LIBS PUBLIC
  MLIRControlFlowInterfaces
  MLIRDataLayoutInterfaces
  MLIRDerivedAttributeOpInterface
  MLIRDestinationStyleOpInterface
  MLIRDialect
  MLIRDLTIDialect
  MLIRFuncDialect
  MLIRFunctionInterfaces
  MLIRFuncTransforms
  MLIRIR
  MLIRInferIntRangeInterface
  MLIRInferTypeOpInterface
  MLIRLinalgDialect
  MLIRLinalgTransforms
  MLIRLLVMDialect
  MLIRPass
  MLIRReduce
  MLIRTensorDialect
  MLIRTransformUtils
  MLIRTransforms
)

add_mlir_translation_library(MLIRTestToLLVMIRTranslation
  TestToLLVMIRTranslation.cpp

  EXCLUDE_FROM_LIBMLIR

  LINK_COMPONENTS
  Core

  LINK_LIBS PUBLIC
  MLIRIR
  MLIRLLVMDialect
  MLIRTestDialect
  MLIRSupport
  MLIRBuiltinToLLVMIRTranslation
  MLIRLLVMToLLVMIRTranslation
)
