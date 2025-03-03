# Create a library to contain the pw_unit_test framwork
add_library(pw_unit_test STATIC)
add_subdirectory(src)
add_subdirectory(include)
platformify(pw_unit_test)
