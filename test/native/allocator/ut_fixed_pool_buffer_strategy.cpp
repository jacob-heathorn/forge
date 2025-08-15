#include "ftl/allocator/fixed_pool_buffer_strategy.hpp"
#include "ftl/allocator/buffer_allocator.hpp"
#include "ftl/allocator/bump_allocator.hpp"
#include "gtest/gtest.h"
#include <vector>
#include <thread>
#include <cstring>

class FixedPoolBufferStrategyTest : public ::testing::Test {
protected:
    void SetUp() override {
    }
    
    void TearDown() override {
    }
};

// Test basic allocation and deallocation
TEST_F(FixedPoolBufferStrategyTest, BasicAllocation) {
    alignas(64) uint8_t arena[8192];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    // Create strategy with 10 buffers of 256 bytes each
    ftl::allocator::FixedPoolBufferStrategy strategy(bump, 256, 10);
    
    // Note: IBufferStrategy stores size_ and alignment_ as protected members
    // We can't directly access them, but we can test their effects
    EXPECT_EQ(strategy.capacity(), 10u);
    EXPECT_EQ(strategy.available(), 10u);
    
    // Allocate a buffer
    ftl::Buffer* buf = strategy.allocate(200);
    ASSERT_NE(buf, nullptr);
    EXPECT_EQ(buf->size(), 200u);  // Should have requested size
    // Buffer doesn't have capacity() method, only size()
    
    EXPECT_EQ(strategy.available(), 9u);
    EXPECT_EQ(strategy.allocated(), 1u);
    
    // Write to buffer
    std::memset(buf->front(), 0xAB, buf->size());
    
    // Deallocate
    strategy.deallocate(buf);
    EXPECT_EQ(strategy.available(), 10u);
    EXPECT_EQ(strategy.allocated(), 0u);
}

// Test pool exhaustion
TEST_F(FixedPoolBufferStrategyTest, PoolExhaustion) {
    alignas(64) uint8_t arena[4096];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    constexpr std::size_t POOL_SIZE = 5;
    ftl::allocator::FixedPoolBufferStrategy strategy(bump, 128, POOL_SIZE);
    
    std::vector<ftl::Buffer*> buffers;
    
    // Allocate all buffers
    for (std::size_t i = 0; i < POOL_SIZE; ++i) {
        ftl::Buffer* buf = strategy.allocate(100);
        ASSERT_NE(buf, nullptr) << "Failed to allocate buffer " << i;
        buffers.push_back(buf);
    }
    
    EXPECT_EQ(strategy.available(), 0u);
    EXPECT_EQ(strategy.allocated(), POOL_SIZE);
    
    // Try to allocate one more - should fail
    ftl::Buffer* extra = strategy.allocate(100);
    EXPECT_EQ(extra, nullptr);
    
    // Free one buffer
    strategy.deallocate(buffers[2]);
    EXPECT_EQ(strategy.available(), 1u);
    
    // Now allocation should succeed
    extra = strategy.allocate(100);
    ASSERT_NE(extra, nullptr);
    
    // Cleanup
    strategy.deallocate(extra);
    for (std::size_t i = 0; i < POOL_SIZE; ++i) {
        if (i != 2) {  // Already freed
            strategy.deallocate(buffers[i]);
        }
    }
    
    EXPECT_EQ(strategy.available(), POOL_SIZE);
}

// Test allocation size limits
TEST_F(FixedPoolBufferStrategyTest, AllocationSizeLimits) {
    alignas(64) uint8_t arena[4096];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    ftl::allocator::FixedPoolBufferStrategy strategy(bump, 100, 5);
    
    // Allocate with size <= buffer_size should succeed
    ftl::Buffer* buf1 = strategy.allocate(50);
    ASSERT_NE(buf1, nullptr);
    EXPECT_EQ(buf1->size(), 50u);
    
    ftl::Buffer* buf2 = strategy.allocate(100);
    ASSERT_NE(buf2, nullptr);
    EXPECT_EQ(buf2->size(), 100u);
    
    // Allocate with size > buffer_size should fail
    ftl::Buffer* buf3 = strategy.allocate(101);
    EXPECT_EQ(buf3, nullptr);
    
    ftl::Buffer* buf4 = strategy.allocate(200);
    EXPECT_EQ(buf4, nullptr);
    
    // Cleanup
    strategy.deallocate(buf1);
    strategy.deallocate(buf2);
}

// Test with BufferAllocator
TEST_F(FixedPoolBufferStrategyTest, WithBufferAllocator) {
    alignas(64) uint8_t arena[16384];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    // Create multiple strategies with different sizes
    auto strategy64 = std::make_unique<ftl::allocator::FixedPoolBufferStrategy>(bump, 64, 10);
    auto strategy128 = std::make_unique<ftl::allocator::FixedPoolBufferStrategy>(bump, 128, 8);
    auto strategy256 = std::make_unique<ftl::allocator::FixedPoolBufferStrategy>(bump, 256, 5);
    
    // Create allocator with strategies
    ftl::allocator::BufferAllocator allocator(
        *strategy64, *strategy128, *strategy256
    );
    
    // Small allocation should use 64-byte strategy
    ftl::Buffer* small = allocator.allocate(32);
    ASSERT_NE(small, nullptr);
    EXPECT_EQ(small->size(), 32u);
    // Buffer size is the requested size
    
    // Medium allocation should use 128-byte strategy
    ftl::Buffer* medium = allocator.allocate(100);
    ASSERT_NE(medium, nullptr);
    EXPECT_EQ(medium->size(), 100u);
    // Buffer size is the requested size
    
    // Large allocation should use 256-byte strategy
    ftl::Buffer* large = allocator.allocate(200);
    ASSERT_NE(large, nullptr);
    EXPECT_EQ(large->size(), 200u);
    // Buffer size is the requested size
    
    // Too large allocation should fail
    ftl::Buffer* too_large = allocator.allocate(300);
    EXPECT_EQ(too_large, nullptr);
    
    // Cleanup
    allocator.deallocate(small);
    allocator.deallocate(medium);
    allocator.deallocate(large);
}

// Test custom alignment
TEST_F(FixedPoolBufferStrategyTest, CustomAlignment) {
    alignas(64) uint8_t arena[8192];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    constexpr std::size_t CACHE_LINE_SIZE = 64;
    ftl::allocator::FixedPoolBufferStrategy strategy(bump, 256, 5, CACHE_LINE_SIZE);
    
    // Allocate buffers and check alignment
    std::vector<ftl::Buffer*> buffers;
    for (int i = 0; i < 5; ++i) {
        ftl::Buffer* buf = strategy.allocate(200);
        ASSERT_NE(buf, nullptr);
        
        // Check that buffer data is aligned
        auto addr = reinterpret_cast<std::uintptr_t>(buf->front());
        EXPECT_EQ(addr % CACHE_LINE_SIZE, 0u) << "Buffer " << i << " not aligned";
        
        buffers.push_back(buf);
    }
    
    // Cleanup
    for (auto* buf : buffers) {
        strategy.deallocate(buf);
    }
}

// Test zero count
TEST_F(FixedPoolBufferStrategyTest, ZeroCount) {
    alignas(64) uint8_t arena[4096];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    ftl::allocator::FixedPoolBufferStrategy strategy(bump, 256, 0);
    
    EXPECT_EQ(strategy.capacity(), 0u);
    EXPECT_EQ(strategy.available(), 0u);
    
    // Allocation should fail
    ftl::Buffer* buf = strategy.allocate(100);
    EXPECT_EQ(buf, nullptr);
}

// Test zero size
TEST_F(FixedPoolBufferStrategyTest, ZeroSize) {
    alignas(64) uint8_t arena[4096];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    ftl::allocator::FixedPoolBufferStrategy strategy(bump, 0, 10);
    
    EXPECT_EQ(strategy.capacity(), 0u);
    
    // Allocation should fail
    ftl::Buffer* buf = strategy.allocate(0);
    EXPECT_EQ(buf, nullptr);
}

// Test bump allocator exhaustion
TEST_F(FixedPoolBufferStrategyTest, BumpAllocatorExhaustion) {
    // Very small arena
    alignas(64) uint8_t small_arena[256];
    ftl::BumpAllocator bump(small_arena, sizeof(small_arena));
    
    // Try to allocate more than fits
    ftl::allocator::FixedPoolBufferStrategy strategy(bump, 256, 100);
    
    // Should have failed to allocate all buffers
    EXPECT_EQ(strategy.capacity(), 0u);
    EXPECT_EQ(strategy.available(), 0u);
    
    // Allocation should fail
    ftl::Buffer* buf = strategy.allocate(100);
    EXPECT_EQ(buf, nullptr);
}

// Test null pointer deallocation
TEST_F(FixedPoolBufferStrategyTest, NullPointerDeallocation) {
    alignas(64) uint8_t arena[4096];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    ftl::allocator::FixedPoolBufferStrategy strategy(bump, 256, 5);
    
    // Deallocating nullptr should be safe
    strategy.deallocate(nullptr);
    EXPECT_EQ(strategy.available(), 5u);
}

// Test reuse after deallocation
TEST_F(FixedPoolBufferStrategyTest, ReuseAfterDeallocation) {
    alignas(64) uint8_t arena[4096];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    ftl::allocator::FixedPoolBufferStrategy strategy(bump, 128, 3);
    
    // Allocate all buffers
    ftl::Buffer* buf1 = strategy.allocate(100);
    ftl::Buffer* buf2 = strategy.allocate(100);
    ftl::Buffer* buf3 = strategy.allocate(100);
    
    ASSERT_NE(buf1, nullptr);
    ASSERT_NE(buf2, nullptr);
    ASSERT_NE(buf3, nullptr);
    
    // Write distinct patterns
    std::memset(buf1->front(), 0x11, buf1->size());
    std::memset(buf2->front(), 0x22, buf2->size());
    std::memset(buf3->front(), 0x33, buf3->size());
    
    // Deallocate in different order
    strategy.deallocate(buf2);
    strategy.deallocate(buf1);
    strategy.deallocate(buf3);
    
    // Reallocate - should get same buffers back (LIFO order from free list)
    ftl::Buffer* new1 = strategy.allocate(100);
    ftl::Buffer* new2 = strategy.allocate(100);
    ftl::Buffer* new3 = strategy.allocate(100);
    
    ASSERT_NE(new1, nullptr);
    ASSERT_NE(new2, nullptr);
    ASSERT_NE(new3, nullptr);
    
    // Due to LIFO free list, we get them in reverse deallocation order
    EXPECT_EQ(new1, buf3);
    EXPECT_EQ(new2, buf1);
    EXPECT_EQ(new3, buf2);
    
    // Cleanup
    strategy.deallocate(new1);
    strategy.deallocate(new2);
    strategy.deallocate(new3);
}

// Test thread safety
TEST_F(FixedPoolBufferStrategyTest, ThreadSafety) {
    alignas(64) uint8_t arena[32768];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    constexpr std::size_t POOL_SIZE = 100;
    constexpr std::size_t NUM_THREADS = 4;
    constexpr std::size_t OPS_PER_THREAD = 1000;
    
    ftl::allocator::FixedPoolBufferStrategy strategy(bump, 256, POOL_SIZE);
    
    auto worker = [&strategy]() {
        std::vector<ftl::Buffer*> my_buffers;
        
        for (std::size_t i = 0; i < OPS_PER_THREAD; ++i) {
            // Try to allocate
            ftl::Buffer* buf = strategy.allocate(200);
            if (buf) {
                // Write to buffer to ensure it's valid
                std::memset(buf->front(), static_cast<uint8_t>(i & 0xFF), buf->size());
                my_buffers.push_back(buf);
                
                // Keep some allocations for a while
                if (my_buffers.size() > 10) {
                    // Free oldest allocation
                    strategy.deallocate(my_buffers.front());
                    my_buffers.erase(my_buffers.begin());
                }
            } else {
                // If allocation failed, free something if we have it
                if (!my_buffers.empty()) {
                    strategy.deallocate(my_buffers.back());
                    my_buffers.pop_back();
                }
            }
        }
        
        // Free all remaining allocations
        for (ftl::Buffer* buf : my_buffers) {
            strategy.deallocate(buf);
        }
    };
    
    // Run threads
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(worker);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // All buffers should be freed
    EXPECT_EQ(strategy.available(), POOL_SIZE);
}