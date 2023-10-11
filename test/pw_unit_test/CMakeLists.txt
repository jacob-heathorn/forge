include(FetchContent)

FetchContent_Declare(
  pw_unit_test_repository
  GIT_REPOSITORY git@github.com:jheathor-k/pw_unit_test.git
  GIT_TAG        1b4ffff7a3601b438b2d254c446b07ad36d1eb1a
)

FetchContent_GetProperties(pw_unit_test_repository)
if(NOT pw_unit_test_repository_POPULATED)
  FetchContent_Populate(pw_unit_test_repository)
  # Pull in firmware
  add_subdirectory(${pw_unit_test_repository_SOURCE_DIR}/firmware ${pw_unit_test_repository_BINARY_DIR}/firmware)
  # Pull in tests (includes pw-ut-framework)
  add_subdirectory(${pw_unit_test_repository_SOURCE_DIR}/test ${pw_unit_test_repository_BINARY_DIR}/test)
endif()

# Compiles a pigweed unit test executable
function(add_pw_test)
  add_executable(${ARGV})
  platformify(${ARGV0})
  target_link_libraries(${ARGV0} pw_unit_test)
  add_test(NAME ${ARGV0} COMMAND ${ARGV0})
endfunction()

# Add a simple test to verify the framework
add_pw_test(pw-ut-simple pw_ut_simple.cpp)
