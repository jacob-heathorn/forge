add_library(ftl INTERFACE)
target_include_directories(ftl INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

file(GLOB_RECURSE FTL_HEADERS CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/include/ftl/*.hpp
)

target_sources(ftl INTERFACE ${FTL_HEADERS})

message(STATUS "FTL headers: ${FTL_HEADERS}")