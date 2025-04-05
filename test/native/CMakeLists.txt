# Add a simple test both ways to verify both the pigweed and googletest frameworks
add_platform_test(gtest-ut-simple ut_simple.cpp)
add_pw_test(pw-ut-simple ut_simple.cpp)

# Other unit tests.
add_platform_test(ut-forge ut_singleton.cpp)
target_link_libraries(ut-forge forge)

# Manual tests
add_executable(hello-world hello_world.cpp)
platformify(hello-world)


