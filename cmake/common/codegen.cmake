# Generate ftl::mmio register headers from a CMSIS-SVD file.
#
# CMSIS = Cortex Microcontroller Software Interface Standard (ARM's API spec
#         for Cortex-M cores).
# SVD   = System View Description (the XML peripheral-description format that
#         lives under CMSIS).
#
# Example:
#   svd_generate_registers(
#     SVD        ${CMAKE_CURRENT_SOURCE_DIR}/peripherals.svd
#     OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated
#     TARGET     my-codegen)
#
# Picks up edits to forge's codegen sources or jinja templates automatically.
function(svd_generate_registers)
  cmake_parse_arguments(ARG "" "SVD;OUTPUT_DIR;TARGET" "" ${ARGN})

  set(dev_python $ENV{PROJECT_ROOT}/.nox/dev/bin/python)
  if(NOT EXISTS ${dev_python})
    message(FATAL_ERROR
      "dev env not found at ${dev_python}. Run `nox -s dev` in $PROJECT_ROOT.")
  endif()

  file(GLOB codegen_sources CONFIGURE_DEPENDS
    $ENV{FORGE_ROOT}/scripts/package/forge/svd/*.py
    $ENV{FORGE_ROOT}/scripts/package/forge/svd/templates/*.jinja2)

  set(stamp ${ARG_OUTPUT_DIR}/.codegen.stamp)
  add_custom_command(
    OUTPUT ${stamp}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${ARG_OUTPUT_DIR}
    COMMAND ${dev_python} -c
      "from forge import RegisterGenerator; RegisterGenerator('${ARG_SVD}', '${ARG_OUTPUT_DIR}').generate()"
    COMMAND ${CMAKE_COMMAND} -E touch ${stamp}
    DEPENDS ${ARG_SVD} ${codegen_sources}
    COMMENT "Generating registers from ${ARG_SVD}"
    VERBATIM)

  add_custom_target(${ARG_TARGET} DEPENDS ${stamp})
endfunction()
