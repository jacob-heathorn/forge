cmake_minimum_required(VERSION 3.27.4)

project(Forge VERSION 1.0 LANGUAGES C CXX ASM)

# Use C++17. Force it, No Extensions (-std=c++17 not -std=gnu++17) to increase portability.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing with ctest
enable_testing()

# Configure CTest to create Testing folder in the build directory
set(CTEST_BINARY_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CTEST_OUTPUT_ON_FAILURE ON)

# Make the platform module accessible from anywhere in the tree.
list(APPEND CMAKE_MODULE_PATH ${PLATFORM_PATH})

# Add the platform module here at the root of the tree.
include(platform)
include(subdirectories)
