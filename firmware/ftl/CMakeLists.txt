add_library(ftl INTERFACE)
target_include_directories(ftl INTERFACE ${CMAKE_CURRENT_LIST_DIR}/include)

file(GLOB_RECURSE FTL_HEADERS CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/include/ftl/*.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/ftl/**/*.hpp
)

target_sources(ftl INTERFACE ${FTL_HEADERS})

# Add ETL (Embedded Template Library)
add_subdirectory(
  $ENV{ETL_ROOT}
  ${CMAKE_BINARY_DIR}/etl
)
target_link_libraries(ftl INTERFACE etl)
