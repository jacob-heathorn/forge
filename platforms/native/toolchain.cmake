# Set the C and C++ compilers
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

# Perform a compiler test with the static library
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Declare the platform path.
set(PLATFORM_PATH ${CMAKE_CURRENT_LIST_DIR})

# By default, CMake provides -O3 -DNDEBUG for release builds, and -g for debug builds. Surprisingly,
# this can override other options provided later. Clear them out.
set(CMAKE_CXX_FLAGS_DEBUG "")
set(CMAKE_CXX_FLAGS_RELEASE "")
