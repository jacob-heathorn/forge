# Add pigweed unit test, and native-main.
add_subdirectory(
  $ENV{FORGE_ROOT}/firmware/pw_unit_test
  ${CMAKE_BINARY_DIR}/forge/firmware/pw_unit_test
)
target_sources(pw_unit_test
  PRIVATE
    pw_ut_main.cpp
)

# Add FTL-native library.
add_library(ftl-native STATIC
  native_udp_socket.cpp
  native_ethernet_interface.cpp
)
target_link_libraries(ftl-native PUBLIC ftl)
platformify(ftl-native)

target_include_directories(ftl-native
  PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
)

# Expose headers for IDEs and consumers
file(GLOB_RECURSE FTL_NATIVE_HEADERS
  CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ftl/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ftl/**/*.hpp
)

target_sources(ftl-native
  PUBLIC
    ${FTL_NATIVE_HEADERS}
)
