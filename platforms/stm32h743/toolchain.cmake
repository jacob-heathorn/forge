set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(TARGET "arm-none-eabi-")

set(ARM_GCC_TOOLCHAIN_PATH "$ENV{ARM_GCC_TOOLCHAIN_PATH}")
file(TO_CMAKE_PATH ${ARM_GCC_TOOLCHAIN_PATH} ARM_GCC_TOOLCHAIN_PATH)
message(STATUS "Using ARM GCC Compiler = ${ARM_GCC_TOOLCHAIN_PATH}")

# Perform a compiler test with the static library
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Use gcc instead of g++ to prevent implicit linking the C++ standard library (-lstdc++). This keeps
# C++ standard library usage header-only. According to
# https://stackoverflow.com/questions/172587/what-is-the-difference-between-g-and-gcc, "g++"" is
# equivalent to "gcc -xc++ -lstdc++ -shared-libgcc"
set(CMAKE_C_COMPILER    ${ARM_GCC_TOOLCHAIN_PATH}/${TARGET}gcc)
set(CMAKE_CXX_COMPILER  ${ARM_GCC_TOOLCHAIN_PATH}/${TARGET}gcc)
set(CMAKE_ASM_COMPILER  ${ARM_GCC_TOOLCHAIN_PATH}/${TARGET}gcc)
set(CMAKE_LINKER        ${ARM_GCC_TOOLCHAIN_PATH}/${TARGET}gcc)
set(CMAKE_SIZE_UTIL     ${ARM_GCC_TOOLCHAIN_PATH}/${TARGET}size)
set(CMAKE_OBJCOPY       ${ARM_GCC_TOOLCHAIN_PATH}/${TARGET}objcopy)
set(CMAKE_OBJDUMP       ${ARM_GCC_TOOLCHAIN_PATH}/${TARGET}objdump)
set(CMAKE_NM_UTIL       ${ARM_GCC_TOOLCHAIN_PATH}/${TARGET}gcc-nm)
set(CMAKE_AR            ${ARM_GCC_TOOLCHAIN_PATH}/${TARGET}gcc-ar)
set(CMAKE_RANLIB        ${ARM_GCC_TOOLCHAIN_PATH}/${TARGET}gcc-ranlib)

# Set options for cross-compiling
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Declare the platform path.
set(PLATFORM_PATH ${CMAKE_CURRENT_LIST_DIR})

# NOTE: Even while using target_compile_options() to explicitly set these flags on each target,
# cmake still generates an error saying they differ. I am not sure if this is a bug, but they need
# to be set here as well. CMake seems to be generating other stuff that links with the targets with
# explicitly defined options and conflicting. Keeping these flags here for now until they are better
# understood. Ultimately the questions is: can I ONLY use target_compile_options() and
# target_link_options() for setting flags? Well, not at the moment I guess.
set(INIT_COMPILE_FLAGS_INIT "-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb")
set(CMAKE_C_FLAGS ${INIT_COMPILE_FLAGS_INIT})
set(CMAKE_CXX_FLAGS ${INIT_COMPILE_FLAGS_INIT})

# By default, CMake provides -O3 -DNDEBUG for release builds, and -g for debug builds. Surprisingly,
# this can override other options provided later. Clear them out.
set(CMAKE_CXX_FLAGS_DEBUG "")
set(CMAKE_CXX_FLAGS_RELEASE "")
