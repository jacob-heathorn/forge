add_library(ftl-native INTERFACE)
target_include_directories(ftl-native INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

file(GLOB_RECURSE FTL_HEADERS CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/include/ftl/*.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/ftl/**/*.hpp
)

target_sources(ftl INTERFACE ${FTL_HEADERS})
