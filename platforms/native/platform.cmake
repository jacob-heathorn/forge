# The directory of this file is added to the module path from project root, enabling direct inclusion
# across the tree.

# Include the common platform cmake.
include(${PROJECT_SOURCE_DIR}/platforms/common/platform.cmake)

# Adds platform-specific libraries and options to the target.
function(platformify target)
  add_common_c_cxx_flags(${target})
endfunction()

# Compiles a pigweed unit test executable
function(add_pw_test)
  add_executable(${ARGV})
  platformify(${ARGV0})
  target_link_libraries(${ARGV0} pw_unit_test)
endfunction()
