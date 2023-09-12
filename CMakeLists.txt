cmake_minimum_required(VERSION 3.27.4)

project(Forge VERSION 1.0 LANGUAGES C CXX ASM)

# Use C++17. Force it, No Extensions (-std=c++17 not -std=gnu++17) to increase portability.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# Make modules accessible. This module extension must be done at
# the top level in order to be visible to the whole source tree.
list(APPEND CMAKE_MODULE_PATH ${PLATFORM_PATH})

add_subdirectory(test)
