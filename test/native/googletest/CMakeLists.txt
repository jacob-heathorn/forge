# # Add this line to help CMake find Google Test installed in a local folder
# list(APPEND CMAKE_PREFIX_PATH "$ENV{FORGE_ROOT}/.cache/googletest-install")

find_package(GTest REQUIRED)

add_executable(ut-simple ut_simple.cpp)
target_link_libraries(ut-simple GTest::GTest GTest::Main)
add_test(NAME ut-simple COMMAND ut-simple)
