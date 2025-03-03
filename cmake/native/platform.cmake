# The directory of this file is added to the module path from project root, enabling direct inclusion
# across the tree.

# Include the common platform cmake.
include(${CMAKE_CURRENT_LIST_DIR}/../common/platform.cmake)

# Adds a gtest unit test executable, which can be executed with ctest.
function(add_platform_test)
  find_package(GTest REQUIRED)
  add_executable(${ARGV})
  platformify(${ARGV0})
  target_link_libraries(${ARGV0} GTest::GTest GTest::Main)
  add_test(NAME ${ARGV0} COMMAND ${ARGV0})
endfunction()

# Adds a pigweed unit test executable, which can be executed with ctest.
function(add_pw_test)
  add_executable(${ARGV})
  platformify(${ARGV0})
  target_link_libraries(${ARGV0} PRIVATE pw_unit_test)
  add_test(NAME ${ARGV0} COMMAND ${ARGV0})
endfunction()

# Adds platform-specific libraries and options to the target.
function(platformify target)
  add_common_c_cxx_flags(${target})
endfunction()
