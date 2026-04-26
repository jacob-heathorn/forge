# End-to-end MMIO test: runs the SVD generator on a fixture SVD and compiles
# a native test against the generated header. Any regression in either the
# ftl::mmio primitives or the code generator surfaces here.

set(TEST_SVD ${CMAKE_CURRENT_SOURCE_DIR}/test_peripheral.svd)
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
# generate() emits both standalone (testperiph.hpp) and family (port.hpp)
# headers from one SVD; we use testperiph.hpp as the OUTPUT sentinel for ninja.
set(GENERATED_HEADER ${GENERATED_DIR}/testperiph.hpp)

# Use forge's nox dev env so `import forge` resolves.
set(FORGE_PYTHON $ENV{FORGE_ROOT}/.nox/dev/bin/python)
if(NOT EXISTS ${FORGE_PYTHON})
  message(FATAL_ERROR
    "forge dev env not found at ${FORGE_PYTHON}. Run `nox -s dev` in $FORGE_ROOT.")
endif()

add_custom_command(
  OUTPUT ${GENERATED_HEADER}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_DIR}
  COMMAND ${FORGE_PYTHON} -c
    "from forge import RegisterGenerator; RegisterGenerator('${TEST_SVD}', '${GENERATED_DIR}').generate()"
  DEPENDS
    ${TEST_SVD}
    $ENV{FORGE_ROOT}/scripts/package/forge/cmsis_svd/register_generator.py
    $ENV{FORGE_ROOT}/scripts/package/forge/cmsis_svd/register_parser.py
    $ENV{FORGE_ROOT}/scripts/package/forge/cmsis_svd/templates/register.jinja2
    $ENV{FORGE_ROOT}/scripts/package/forge/cmsis_svd/templates/peripheral.jinja2
    $ENV{FORGE_ROOT}/scripts/package/forge/cmsis_svd/templates/family.jinja2
  COMMENT "Generating testperiph.hpp from test_peripheral.svd"
  VERBATIM
)

add_custom_target(ut-mmio-generated DEPENDS ${GENERATED_HEADER})

add_platform_test(ut-mmio ut_mmio.cpp)
add_dependencies(ut-mmio ut-mmio-generated)
target_include_directories(ut-mmio PRIVATE ${GENERATED_DIR})
target_link_libraries(ut-mmio ftl)
