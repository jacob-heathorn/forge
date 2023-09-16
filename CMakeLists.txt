cmake_minimum_required(VERSION 3.27.4)

project(Forge VERSION 1.0 LANGUAGES C CXX ASM)

# Use C++17. Force it, No Extensions (-std=c++17 not -std=gnu++17) to increase portability.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# Make the platform module accessible from anywhere in the tree.
list(APPEND CMAKE_MODULE_PATH ${PLATFORM_PATH})

# Add the platform module here at the root of the tree.
include(platform)

add_subdirectory(test)
