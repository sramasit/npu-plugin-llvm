//===-- Passes.td - Test pass definition file --------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef MLIR_DIALECT_LINALG_PASSES
#define MLIR_DIALECT_LINALG_PASSES

include "mlir/Pass/PassBase.td"

def TestBufferize : Pass<"test-bufferize"> {
  let summary = "Bufferize the Test dialect";
  let description = [{
      Run one-shot bufferization for test dialect ops.
  }];
  let constructor = "test::createTestBufferizePass()";
  let dependentDialects = [
    "mlir::bufferization::BufferizationDialect"
  ];
}

#endif // MLIR_DIALECT_LINALG_PASSES
