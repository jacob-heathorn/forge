# Add a simple test both ways to verify both the pigweed and googletest frameworks
add_platform_test(gtest-ut-simple ut_simple.cpp)
add_pw_test(pw-ut-simple ut_simple.cpp)

# FTL unit tests.
add_platform_test(ut-ftl
  ut_buffer_bump_pool.cpp
  ut_bump_allocator.cpp
  ut_bump_pool.cpp
  ut_byte_order.cpp
  ut_map.cpp
  ut_singleton.cpp
  ut_mutex.cpp
  ut_thread.cpp
  ut_unique_bump_pool.cpp
)
target_link_libraries(ut-ftl ftl ftl-native)

# Allocator unit tests
add_platform_test(ut-allocator
  allocator/ut_malloc_strategy.cpp
  allocator/ut_bump_pool_strategy.cpp
  allocator/ut_obj_allocator.cpp
  allocator/ut_malloc_buffer_strategy.cpp
)
target_link_libraries(ut-allocator ftl ftl-native)

# Network test
add_platform_test(ut-network
  ut_udp_socket.cpp
)
target_link_libraries(ut-network ftl ftl-native)

# Manual tests
add_executable(hello-world hello_world.cpp)
platformify(hello-world)

add_executable(hello-udp hello_udp.cpp)
platformify(hello-udp)
target_link_libraries(hello-udp ftl ftl-native)

add_executable(hello-udp2 hello_udp2.cpp)
platformify(hello-udp2)
target_link_libraries(hello-udp2 ftl ftl-native)

add_executable(test-map-performance test_map_performance.cpp)
platformify(test-map-performance)
target_link_libraries(test-map-performance ftl ftl-native)
