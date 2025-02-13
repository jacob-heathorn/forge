add_subdirectory(pw_unit_test)
add_subdirectory(googletest)

add_executable(hello-world hello_world.cpp)
platformify(hello-world)
