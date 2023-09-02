# The directory of this file is added to the module path from project root, enabling direct inclusion
# across the tree.

# Include the common platform cmake.
include(${PROJECT_SOURCE_DIR}/cmake/common_platform.cmake)

# Wrapper for add_executable
function(App)
  set(APP_NAME ${ARGV0})
  add_executable(${ARGN})
  set_target_properties(${APP_NAME} PROPERTIES OUTPUT_FORMAT elf)
  target_link_libraries(${APP_NAME} startup)
  target_link_libraries(${APP_NAME} new_delete)
  add_common_target_flags(${APP_NAME})
  target_include_directories(stm32h743_registers PUBLIC ${PROJECT_SOURCE_DIR})
endfunction()

# Wrapper for add_library
function(Lib)
  add_library(${ARGN})
  set(LIB_NAME ${ARGV0})
  target_link_libraries(${LIB_NAME} PRIVATE new_delete)
  add_common_target_flags(${LIB_NAME})
  target_include_directories(stm32h743_registers PUBLIC ${PROJECT_SOURCE_DIR})
endfunction()

# Compiles a unit test executable
function(Test)
  App(${ARGN})
  set(LIB_NAME ${ARGV0})
  target_link_libraries(${LIB_NAME} pw_unit_test)
endfunction()

# Add common target compile and link flags to the target.
function(add_common_target_flags target)
  
  # Linker flags
  target_link_options(${target} PRIVATE
    --specs=nosys.specs              # Redirects system calls to stub functions
    #--specs=nano.specs              # Links against a smaller version of C standard library
    -static                          # Links libraries statically, not dynamically
    -Wl,--start-group -lc -lm -Wl,--end-group  # Wraps system libraries in a group to resolve circular dependencies
    -Wl,--gc-sections                # Enables garbage collection of unused input sections
  )
  
  # Compiler flags
  target_compile_options(${target} PRIVATE
    -mcpu=cortex-m7            # Specifies the target processor (Cortex-M7)
    -mfpu=fpv5-d16             # Specifies the floating-point hardware (FPv5-D16)
    -mfloat-abi=hard           # Specifies that we are using hardware floating-point instructions
    -mthumb                    # Enables generation of Thumb (compressed) instructions
    -fno-exceptions            # Disables exceptions in C++
    -fno-rtti                  # Disables Run-Time Type Information (RTTI) in C++
    -fno-use-cxa-atexit        # Avoids registering destructors for global/static objects with __cxa_atexit
  )

  # Add common flags
  add_common_c_cxx_flags(${target})
endfunction()
