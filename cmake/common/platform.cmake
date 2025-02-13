function(add_common_c_cxx_flags target)
  # C/C++ warnings.
  #
  # -Wall: Enables a wide range of warnings for questionable code constructions.
  # -Wextra: Activates additional warning flags not covered by -Wall.
  # -Werror: Turns all warnings into errors, stopping compilation at the first warning.
  # -Wcast-align: Alerts on potentially problematic pointer casts due to alignment requirements.
  # -Wformat-security: Warns of potential security issues in format functions.
  # -Werror=sign-conversion: Errors on implicit conversions that might alter an integer's sign.
  # -Winvalid-pch: Warns when a precompiled header (PCH) file is unusable because it's invalid.
  # -Wmissing-format-attribute: Warns about function pointers suitable for format attributes.
  # -Wnull-dereference: Alerts when a null pointer dereferencing might lead to erroneous behavior.
  # -Wpacked: Warns on usage of the packed attribute when it doesn't affect structure layout or size.
  # -Wpointer-arith: Warns on operations that depend on the "size of" a function type or void.
  # -Wredundant-decls: Alerts about redundant declarations in the same scope.
  # -Wsign-compare: Warns when a comparison between signed and unsigned values could lead to incorrect results.
  # -Wdouble-promotion: Warns of implicit promotion of float values to double.
  # -Wswitch-default: Warns when a switch statement lacks a default case.
  # -Wswitch-enum: Warns when a switch statement doesn't handle all enumerated values.
  # -Wundef: Warns when an undefined identifier is used in an #if directive.
  # -Wunused: Enables warnings for unused elements (variables, functions, labels, etc.).
  # -Wwrite-strings: Modifies string literal types to const char [] to catch accidental writes.
  # -Wshadow: Warns when a local variable shadows another entity of the same name.
  # -Wduplicated-cond: Enabled by -Wextra. Warns about duplicated conditions.
  # -Wmisleading-indentation: Warns when the indentation doesn't reflect the control flow.
  # -Wunused-but-set-parameter: Warns about parameters that are assigned but not used afterwards.
  target_compile_options(${target} PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wcast-align
    -Wformat-security
    -Werror=sign-conversion
    -Winvalid-pch
    -Wmissing-format-attribute
    -Wnull-dereference
    -Wpacked
    -Wpointer-arith
    -Wredundant-decls
    -Wsign-compare
    -Wdouble-promotion
    -Wswitch-default
    -Wswitch-enum
    -Wundef
    -Wunused
    -Wwrite-strings
    -Wshadow
    -Wduplicated-cond
    -Wmisleading-indentation
    -Wunused-but-set-parameter
  )

  # -ffunction-sections: Puts each function into its own unique section in the object file.
  # -fdata-sections: Places data items into individual sections, helping linker optimizations.
  target_compile_options(${target} PRIVATE
    -ffunction-sections
    -fdata-sections
  )
  
  # Release mode flags
  #
  # -O3: Enables all -O2 optimizations and additional ones. May increase performance.
  # -DNDEBUG: Disables assert debugging checks in the code. Useful for release versions.
  target_compile_options(${target} PRIVATE
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
  )

  # Debug mode flags
  #
  # -O0: Optimized for debugging.
  # -ggdb: Produces debugging information for use by GDB debugger.
  target_compile_options(${target} PRIVATE
    $<$<CONFIG:Debug>:-Og -ggdb>
  )

endfunction()

#================
# -Wall expanded
#================
# -Waddress: Warns about suspicious uses of addresses.
# -Warray-bounds (only with -O2): Checks array indices are within bounds.
# -Wbool-compare: Warns on redundant comparisons with a boolean.
# -Wbool-operation: Warns about suspicious operations on expressions of boolean type.
# -Wc++11-compat: Warns about features not compatible with C++11.
# -Wc++14-compat: Warns about features not compatible with C++14.
# -Wcatch-value (C++ and Objective-C++ only): Warns on deprecated catch declarations.
# -Wchar-subscripts: Warns if array subscript has type char.
# -Wcomment: Warns about nested comments or dangerous end of line comments.
# -Wduplicate-decl-specifier (C and Objective-C only): Warns on duplicate declaration specifiers.
# -Wenum-compare (in C/ObjC; this is on by default in C++): Warns on comparison between different enums.
# -Wformat: Checks printf and scanf formats.
# -Wint-in-bool-context: Warns about suspicious use of integer values in boolean context.
# -Wimplicit-int (C and Objective-C only): Warns when a declaration does not specify a type.
# -Wimplicit-function-declaration (C and Objective-C only): Warns about implicit function declaration.
# -Winit-self (only for C++): Warns on uninitialized variables that are initialized with themselves.
# -Wlogical-not-parentheses: Warns about logical not used on the left hand side operand of a comparison.
# -Wmain (only for C and ObjC and unless -ffreestanding): Checks the definition of the main function.
# -Wmaybe-uninitialized: Warns if a variable might be uninitialized.
# -Wmemset-transposed-args: Warns if the arguments to memset might be transposed.
# -Wmisleading-indentation (only for C/C++): Warns if indentation implies blocks where there are none.
# -Wmissing-braces (only for C/ObjC): Warns about possibly missing braces around initializers.
# -Wnarrowing (only for C++): Warns about narrowing type conversions.
# -Wnonnull: Warns if a function may be called with null pointer argument.
# -Wopenmp-simd: Warns if constructs prevent vectorization with OpenMP SIMD.
# -Wparentheses: Warns about parentheses in suspicious places.
# -Wpointer-sign: Warns about different signedness between pointer types in assignment or function argument.
# -Wreorder: Warns about code reordering in constructors or initializers (C++ only).
# -Wreturn-type: Warns if a function might not return a value.
# -Wsequence-point: Warns about code where the order of execution is not clear.
# -Wsign-compare (only in C++): Warns when comparing signed to unsigned numbers.
# -Wsizeof-pointer-memaccess: Warns about suspicious uses of sizeof on pointer types.
# -Wstrict-aliasing: Warn
