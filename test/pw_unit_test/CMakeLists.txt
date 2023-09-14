include(FetchContent)
FetchContent_Declare(
  pw_unit_test_repository
  GIT_REPOSITORY git@github.com:jheathor-k/pw_unit_test.git
  GIT_TAG        b0ff210f6508890a0484c08e04d00c80387404b6
  # SOURCE_SUBDIR  cmake
)

FetchContent_MakeAvailable(pw_unit_test_repository)
platformify(pw_unit_test)

# Test the framework
add_pw_test(simple-test.elf simple_test.cpp)
