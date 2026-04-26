# End-to-end MMIO test: runs the SVD generator on a fixture SVD and compiles
# a native test against the generated header. Any regression in either the
# ftl::mmio primitives or the code generator surfaces here.

set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

svd_generate_registers(
  SVD        ${CMAKE_CURRENT_SOURCE_DIR}/test_peripheral.svd
  OUTPUT_DIR ${GENERATED_DIR}
  TARGET     ut-mmio-generated)

add_platform_test(ut-mmio ut_mmio.cpp)
add_dependencies(ut-mmio ut-mmio-generated)
target_include_directories(ut-mmio PRIVATE ${GENERATED_DIR})
target_link_libraries(ut-mmio ftl)
