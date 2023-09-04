# The directory of this file is added to the module path from project root, enabling direct inclusion
# across the tree.

# Include the common platform cmake.
include(${PROJECT_SOURCE_DIR}/platforms/common/platform.cmake)

# Wrapper for add_executable
function(App)
  set(APP_NAME ${ARGV0})
  add_executable(${ARGN})
  add_common_c_cxx_flags(${APP_NAME})
endfunction()

# Wrapper for add_library
function(Lib)
  add_library(${ARGN})
  set(LIB_NAME ${ARGV0})
  add_common_c_cxx_flags(${LIB_NAME})
endfunction()

# TODO
# # Compiles a unit test executable
# function(Test)
#   App(${ARGN})
#   set(LIB_NAME ${ARGV0})
#   target_link_libraries(${LIB_NAME} pw_unit_test)
# endfunction()
