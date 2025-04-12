include_guard(GLOBAL)
# Firmware
add_subdirectory(
  $ENV{FORGE_ROOT}/firmware/ftl
  ${CMAKE_BINARY_DIR}/forge/firmware/ftl
)
add_subdirectory(
  $ENV{FORGE_ROOT}/firmware/ftl_native
  ${CMAKE_BINARY_DIR}/forge/firmware/ftl_native
)

# Test
add_subdirectory(
  $ENV{FORGE_ROOT}/test/common
  ${CMAKE_BINARY_DIR}/forge/test/common
)
add_subdirectory(
  $ENV{FORGE_ROOT}/test/native
  ${CMAKE_BINARY_DIR}/forge/test/native
)
