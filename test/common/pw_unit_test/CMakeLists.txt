# Create a library to contain the pw_unit_test framwork
add_library(pw_unit_test STATIC)
add_subdirectory(src)
add_subdirectory(include)
platformify(pw_unit_test)

# # Compiles a pigweed unit test executable
# function(add_pw_test)
#   add_executable(${ARGV})
#   platformify(${ARGV0})
#   target_link_libraries(${ARGV0} PRIVATE pw_unit_test)
#   add_test(NAME ${ARGV0} COMMAND ${ARGV0})
# endfunction()

# # Add a simple test to verify the framework
# add_pw_test(pw-ut-simple pw_ut_simple.cpp)
