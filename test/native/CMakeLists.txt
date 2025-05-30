# Add a simple test both ways to verify both the pigweed and googletest frameworks
add_platform_test(gtest-ut-simple ut_simple.cpp)
add_pw_test(pw-ut-simple ut_simple.cpp)

# FTL unit tests.
add_platform_test(ut-ftl
  ut_buffer_bump_pool.cpp
  ut_bump_allocator.cpp
  ut_bump_pool.cpp
  ut_singleton.cpp
  ut_mutex.cpp
  ut_thread.cpp
  ut_unique_bump_pool.cpp
)
target_link_libraries(ut-ftl ftl ftl-native)

# # Network test
# add_platform_test(ut-network
#   ut_udp_socket.cpp
# )
# target_link_libraries(ut-network ftl ftl-native)

# Manual tests
add_executable(hello-world hello_world.cpp)
platformify(hello-world)

add_executable(hello-udp hello_udp.cpp)
platformify(hello-udp)
target_link_libraries(hello-udp ftl ftl-native)
