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
add_library(ftl-native INTERFACE)
target_include_directories(ftl-native INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

file(GLOB_RECURSE FTL_HEADERS CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/include/ftl/*.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/ftl/**/*.hpp
)

target_sources(ftl INTERFACE ${FTL_HEADERS})
