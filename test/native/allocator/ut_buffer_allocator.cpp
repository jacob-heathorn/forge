#include "ftl/allocator/buffer_allocator.hpp"
#include "ftl/allocator/malloc_buffer_strategy.hpp"
#include "ftl/allocator/bump_pool_buffer_strategy.hpp"
#include "ftl/allocator/fixed_pool_buffer_strategy.hpp"
#include "ftl/allocator/bump_allocator.hpp"
#include "gtest/gtest.h"
#include <cstring>
#include <vector>

class BufferAllocatorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test basic allocation with MallocBufferStrategy
TEST_F(BufferAllocatorTest, BasicAllocation) {
    // Create individual strategies
    ftl::allocator::MallocBufferStrategy strategy64(64);
    ftl::allocator::MallocBufferStrategy strategy128(128);
    ftl::allocator::MallocBufferStrategy strategy256(256);
    
    ftl::allocator::BufferAllocator allocator(
        strategy64,
        strategy128,
        strategy256
    );
    
    // Allocate a small buffer
    ftl::Buffer* buf1 = allocator.allocate(32);
    ASSERT_NE(buf1, nullptr);
    EXPECT_EQ(buf1->size(), 32);  // Buffer reports requested size
    
    // Write to the buffer
    std::memset(buf1->front(), 0xAB, buf1->size());
    
    // Allocate a medium buffer
    ftl::Buffer* buf2 = allocator.allocate(100);
    ASSERT_NE(buf2, nullptr);
    EXPECT_EQ(buf2->size(), 100);
    
    // Allocate a large buffer
    ftl::Buffer* buf3 = allocator.allocate(200);
    ASSERT_NE(buf3, nullptr);
    EXPECT_EQ(buf3->size(), 200);
    
    // Cleanup
    allocator.deallocate(buf1);
    allocator.deallocate(buf2);
    allocator.deallocate(buf3);
}

// Test accessor methods
TEST_F(BufferAllocatorTest, Accessors) {
    ftl::allocator::MallocBufferStrategy strategy32(32);
    ftl::allocator::MallocBufferStrategy strategy64(64);
    ftl::allocator::MallocBufferStrategy strategy128(128);
    
    ftl::allocator::BufferAllocator allocator(
        strategy32,
        strategy64,
        strategy128
    );
    
    const auto& sizes = allocator.sizes();
    EXPECT_EQ(sizes[0], 32);
    EXPECT_EQ(sizes[1], 64);
    EXPECT_EQ(sizes[2], 128);
    
    EXPECT_EQ(allocator.num_slots(), 3u);
}

// Test null pointer handling
TEST_F(BufferAllocatorTest, NullPointerHandling) {
    ftl::allocator::MallocBufferStrategy strategy64(64);
    
    ftl::allocator::BufferAllocator allocator(strategy64);
    
    // Deallocating nullptr should be safe
    allocator.deallocate(nullptr);
    
    // Allocate and deallocate normally
    ftl::Buffer* buf = allocator.allocate(32);
    ASSERT_NE(buf, nullptr);
    allocator.deallocate(buf);
}

// Test multiple allocations and deallocations
TEST_F(BufferAllocatorTest, MultipleAllocations) {
    ftl::allocator::MallocBufferStrategy strategy64(64);
    ftl::allocator::MallocBufferStrategy strategy128(128);
    
    ftl::allocator::BufferAllocator allocator(strategy64, strategy128);
    
    std::vector<ftl::Buffer*> buffers;
    
    // Allocate several buffers
    for (int i = 0; i < 10; ++i) {
        size_t size = (i % 2 == 0) ? 32 : 100;
        ftl::Buffer* buf = allocator.allocate(size);
        ASSERT_NE(buf, nullptr);
        EXPECT_EQ(buf->size(), size);
        buffers.push_back(buf);
    }
    
    // Deallocate all
    for (ftl::Buffer* buf : buffers) {
        allocator.deallocate(buf);
    }
}

// Test allocation too large
TEST_F(BufferAllocatorTest, AllocationTooLarge) {
    ftl::allocator::MallocBufferStrategy strategy64(64);
    ftl::allocator::MallocBufferStrategy strategy128(128);
    
    ftl::allocator::BufferAllocator allocator(strategy64, strategy128);
    
    // Try to allocate larger than max size
    ftl::Buffer* buf = allocator.allocate(256);
    EXPECT_EQ(buf, nullptr);
}

// Test custom alignment
TEST_F(BufferAllocatorTest, CustomAlignment) {
    constexpr size_t CACHE_LINE_SIZE = 64;
    
    ftl::allocator::MallocBufferStrategy strategy128(128, CACHE_LINE_SIZE);
    ftl::allocator::MallocBufferStrategy strategy256(256, CACHE_LINE_SIZE);
    
    ftl::allocator::BufferAllocator allocator(strategy128, strategy256);
    
    ftl::Buffer* buf1 = allocator.allocate(100);
    ASSERT_NE(buf1, nullptr);
    
    ftl::Buffer* buf2 = allocator.allocate(200);
    ASSERT_NE(buf2, nullptr);
    
    // Check alignment
    auto addr1 = reinterpret_cast<std::uintptr_t>(buf1->front());
    auto addr2 = reinterpret_cast<std::uintptr_t>(buf2->front());
    
    EXPECT_EQ(addr1 % CACHE_LINE_SIZE, 0);
    EXPECT_EQ(addr2 % CACHE_LINE_SIZE, 0);
    
    allocator.deallocate(buf1);
    allocator.deallocate(buf2);
}

// Test with sizes in non-ascending order (should be sorted by allocator)
TEST_F(BufferAllocatorTest, NonAscendingOrder) {
    // Create strategies in random order
    ftl::allocator::MallocBufferStrategy strategy256(256);
    ftl::allocator::MallocBufferStrategy strategy64(64);
    ftl::allocator::MallocBufferStrategy strategy512(512);
    ftl::allocator::MallocBufferStrategy strategy128(128);
    
    ftl::allocator::BufferAllocator allocator(
        strategy256, strategy64, strategy512, strategy128
    );
    
    // Verify sizes are sorted
    const auto& sorted_sizes = allocator.sizes();
    EXPECT_EQ(sorted_sizes[0], 64);
    EXPECT_EQ(sorted_sizes[1], 128);
    EXPECT_EQ(sorted_sizes[2], 256);
    EXPECT_EQ(sorted_sizes[3], 512);
    
    // Test allocation still works correctly
    ftl::Buffer* buf1 = allocator.allocate(50);
    ASSERT_NE(buf1, nullptr);
    EXPECT_EQ(buf1->size(), 50);  // Buffer reports requested size
    
    ftl::Buffer* buf2 = allocator.allocate(200);
    ASSERT_NE(buf2, nullptr);
    EXPECT_EQ(buf2->size(), 200);
    
    allocator.deallocate(buf1);
    allocator.deallocate(buf2);
}

// Test with BumpPoolBufferStrategy
TEST_F(BufferAllocatorTest, WithBumpPoolStrategy) {
    alignas(64) uint8_t arena[8192];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    // Create bump pool strategies with different sizes
    ftl::allocator::BumpPoolBufferStrategy strategy64(bump, 64);
    ftl::allocator::BumpPoolBufferStrategy strategy128(bump, 128);
    ftl::allocator::BumpPoolBufferStrategy strategy256(bump, 256);
    
    ftl::allocator::BufferAllocator allocator(
        strategy64,
        strategy128,
        strategy256
    );
    
    // Test allocation
    ftl::Buffer* small = allocator.allocate(32);
    ASSERT_NE(small, nullptr);
    EXPECT_EQ(small->size(), 32);
    
    ftl::Buffer* medium = allocator.allocate(100);
    ASSERT_NE(medium, nullptr);
    EXPECT_EQ(medium->size(), 100);
    
    ftl::Buffer* large = allocator.allocate(200);
    ASSERT_NE(large, nullptr);
    EXPECT_EQ(large->size(), 200);
    
    // Deallocate and reallocate to test free list
    allocator.deallocate(medium);
    
    ftl::Buffer* medium2 = allocator.allocate(90);
    ASSERT_NE(medium2, nullptr);
    EXPECT_EQ(medium2->size(), 90);
    
    // Cleanup
    allocator.deallocate(small);
    allocator.deallocate(medium2);
    allocator.deallocate(large);
}

// Test with FixedPoolBufferStrategy
TEST_F(BufferAllocatorTest, WithFixedPoolStrategy) {
    alignas(64) uint8_t arena[16384];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    // Create fixed pool strategies with different sizes
    // Each pool pre-allocates a fixed number of buffers
    ftl::allocator::FixedPoolBufferStrategy strategy64(bump, 64, 10);
    ftl::allocator::FixedPoolBufferStrategy strategy128(bump, 128, 8);
    ftl::allocator::FixedPoolBufferStrategy strategy256(bump, 256, 5);
    
    ftl::allocator::BufferAllocator allocator(
        strategy64,
        strategy128,
        strategy256
    );
    
    // Test allocation
    ftl::Buffer* small = allocator.allocate(32);
    ASSERT_NE(small, nullptr);
    EXPECT_EQ(small->size(), 32);
    
    ftl::Buffer* medium = allocator.allocate(100);
    ASSERT_NE(medium, nullptr);
    EXPECT_EQ(medium->size(), 100);
    
    ftl::Buffer* large = allocator.allocate(200);
    ASSERT_NE(large, nullptr);
    EXPECT_EQ(large->size(), 200);
    
    // Test pool limits - allocate many small buffers
    std::vector<ftl::Buffer*> smallBuffers;
    for (int i = 0; i < 9; ++i) {  // Already allocated 1, pool size is 10
        ftl::Buffer* buf = allocator.allocate(32);
        if (buf) {
            smallBuffers.push_back(buf);
        }
    }
    EXPECT_EQ(smallBuffers.size(), 9u);  // Should get 9 more (total 10)
    
    // Try one more - should fail (pool exhausted)
    ftl::Buffer* extra = allocator.allocate(32);
    EXPECT_EQ(extra, nullptr);
    
    // But we can still allocate from other pools
    ftl::Buffer* medium2 = allocator.allocate(100);
    ASSERT_NE(medium2, nullptr);
    
    // Cleanup
    allocator.deallocate(small);
    allocator.deallocate(medium);
    allocator.deallocate(medium2);
    allocator.deallocate(large);
    for (auto* buf : smallBuffers) {
        allocator.deallocate(buf);
    }
}

// Test mixed strategies (malloc, bump pool, and fixed pool)
TEST_F(BufferAllocatorTest, MixedStrategies) {
    alignas(64) uint8_t arena[8192];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    // Mix of different strategy types
    ftl::allocator::MallocBufferStrategy malloc64(64);
    ftl::allocator::BumpPoolBufferStrategy bumpPool128(bump, 128);
    ftl::allocator::FixedPoolBufferStrategy fixedPool256(bump, 256, 5);
    
    ftl::allocator::BufferAllocator allocator(
        malloc64,
        bumpPool128,
        fixedPool256
    );
    
    // Test allocation from each strategy
    ftl::Buffer* small = allocator.allocate(32);    // Should use malloc strategy
    ASSERT_NE(small, nullptr);
    EXPECT_EQ(small->size(), 32);
    
    ftl::Buffer* medium = allocator.allocate(100);  // Should use bump pool strategy
    ASSERT_NE(medium, nullptr);
    EXPECT_EQ(medium->size(), 100);
    
    ftl::Buffer* large = allocator.allocate(200);   // Should use fixed pool strategy
    ASSERT_NE(large, nullptr);
    EXPECT_EQ(large->size(), 200);
    
    // Verify proper cleanup through allocator
    allocator.deallocate(small);
    allocator.deallocate(medium);
    allocator.deallocate(large);
    
    // Allocate again to verify strategies are still working
    ftl::Buffer* small2 = allocator.allocate(50);
    ASSERT_NE(small2, nullptr);
    
    ftl::Buffer* medium2 = allocator.allocate(120);
    ASSERT_NE(medium2, nullptr);
    
    ftl::Buffer* large2 = allocator.allocate(250);
    ASSERT_NE(large2, nullptr);
    
    allocator.deallocate(small2);
    allocator.deallocate(medium2);
    allocator.deallocate(large2);
}