add_library(ftl-threadx INTERFACE)
target_include_directories(ftl-threadx INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

file(GLOB_RECURSE FTL_HEADERS CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/include/ftl/*.hpp
)

target_sources(ftl-threadx INTERFACE ${FTL_HEADERS})

