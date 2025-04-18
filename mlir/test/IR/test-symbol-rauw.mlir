// RUN: mlir-opt -allow-unregistered-dialect -mlir-print-local-scope %s -test-symbol-rauw -split-input-file | FileCheck %s

// Symbol references to the module itself don't affect uses of symbols within
// its table.
// CHECK: module
// CHECK-SAME: @symbol_foo
module attributes {sym.outside_use = @symbol_foo } {
  // CHECK: func private @replaced_foo
  func.func private @symbol_foo() attributes {sym.new_name = "replaced_foo" }

  // CHECK: func @symbol_bar
  // CHECK: @replaced_foo
  func.func @symbol_bar() attributes {sym.use = @symbol_foo} {
    // CHECK: foo.op
    // CHECK-SAME: non_symbol_attr,
    // CHECK-SAME: use = [{nested_symbol = [@replaced_foo], other_use = @symbol_bar, z_use = @replaced_foo}],
    // CHECK-SAME: z_non_symbol_attr_3
    "foo.op"() {
      non_symbol_attr,
      use = [{nested_symbol = [@symbol_foo], other_use = @symbol_bar, z_use = @symbol_foo}],
      z_non_symbol_attr_3
    } : () -> ()
  }

  // CHECK: module attributes {test.reference = @replaced_foo}
  module attributes {test.reference = @symbol_foo} {
    // CHECK: foo.op
    // CHECK-SAME: @symbol_foo
    "foo.op"() {test.nested_reference = @symbol_foo} : () -> ()
  }
}

// -----

// Check the support for nested references.

// CHECK: module
module {
  // CHECK: module @module_a
  module @module_a {
    // CHECK: func nested @replaced_foo
    func.func nested @foo() attributes {sym.new_name = "replaced_foo" }
  }

  // CHECK: module @replaced_module_b
  module @module_b attributes {sym.new_name = "replaced_module_b"} {
    // CHECK: module @replaced_module_c
    module @module_c attributes {sym.new_name = "replaced_module_c"} {
      // CHECK: func nested @replaced_foo
      func.func nested @foo() attributes {sym.new_name = "replaced_foo" }
    }
  }

  // FIXME:#73140
  // DISABLED-CHECK: func @symbol_bar
  func.func @symbol_bar() {
    // DISABLED-CHECK: foo.op
    // DISABLED-CHECK-SAME: use_1 = @module_a::@replaced_foo
    // DISABLED-CHECK-SAME: use_2 = @replaced_module_b::@replaced_module_c::@replaced_foo
    "foo.op"() {
      use_1 = @module_a::@foo,
      use_2 = @module_b::@module_c::@foo
    } : () -> ()
  }
}

// -----

// Check that the replacement fails for potentially unknown symbol tables.
module {
  // CHECK: func private @failed_repl
  func.func private @failed_repl() attributes {sym.new_name = "replaced_name" }

  "foo.possibly_unknown_symbol_table"() ({
  }) : () -> ()
}

// -----

// Check that replacement works in any implementations of SubElements.
module {
    // CHECK: func private @replaced_foo
    func.func private @symbol_foo() attributes {sym.new_name = "replaced_foo" }

    // CHECK: func @symbol_bar
    func.func @symbol_bar() {
      // CHECK: foo.op
      // CHECK-SAME: non_symbol_attr,
      // CHECK-SAME: use = [#test.sub_elements_access<[@replaced_foo], @symbol_bar, @replaced_foo>, distinct[0]<@replaced_foo>],
      // CHECK-SAME: z_non_symbol_attr_3
      "foo.op"() {
        non_symbol_attr,
        use = [#test.sub_elements_access<[@symbol_foo],@symbol_bar,@symbol_foo>, distinct[0]<@symbol_foo>],
        z_non_symbol_attr_3
      } : () -> ()
    }
}

// -----

// FIXME:#73140
module {
  // DISABLED-CHECK: module @replaced_foo
  module @foo attributes {sym.new_name = "replaced_foo" } {
    // DISABLED-CHECK: func.func private @foo
    func.func private @foo()
  }

  // DISABLED-CHECK: foo.op
  // DISABLED-CHECK-SAME: use = @replaced_foo::@foo
  "foo.op"() {
    use = @foo::@foo
  } : () -> ()
}
