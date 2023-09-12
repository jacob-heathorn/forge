# The directory of this file is added to the module path from project root, enabling direct inclusion
# across the tree.

# Include the common platform cmake.
include(${PROJECT_SOURCE_DIR}/platforms/common/platform.cmake)

function(platformify target)
  add_common_c_cxx_flags(${target})
endfunction()
