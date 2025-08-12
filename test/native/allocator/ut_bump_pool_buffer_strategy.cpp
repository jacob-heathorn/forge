#include "ftl/allocator/bump_pool_buffer_strategy.hpp"
#include "ftl/bump_allocator.hpp"
#include "gtest/gtest.h"
#include <cstring>
#include <memory>
#include <vector>

class BumpPoolBufferStrategyTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::memset(arena_, 0, sizeof(arena_));
        bump_allocator_ = std::make_unique<ftl::BumpAllocator>(arena_, sizeof(arena_));
    }
    
    void TearDown() override {
        bump_allocator_.reset();
    }
    
    alignas(64) uint8_t arena_[16384];  // 16KB arena
    std::unique_ptr<ftl::BumpAllocator> bump_allocator_;
};

// Test basic allocation and deallocation
TEST_F(BumpPoolBufferStrategyTest, BasicAllocation) {
    std::array<std::size_t, 3> sizes = {64, 128, 256};
    ftl::allocator::BumpPoolBufferStrategy<3> strategy(*bump_allocator_, sizes);
    
    // Allocate a small buffer
    ftl::Buffer* buf1 = strategy.allocate(32);
    ASSERT_NE(buf1, nullptr);
    EXPECT_EQ(buf1->size(), 64);
    EXPECT_NE(buf1->front(), nullptr);
    
    // Write to buffer to verify it's valid
    std::memset(buf1->front(), 0xAA, buf1->size());
    
    // Deallocate (goes to free list)
    strategy.deallocate(buf1);
    
    // Allocate again - should reuse from free list
    ftl::Buffer* buf2 = strategy.allocate(32);
    ASSERT_NE(buf2, nullptr);
    EXPECT_EQ(buf2, buf1);  // Should get same buffer back
    
    strategy.deallocate(buf2);
}

// Test free list reuse
TEST_F(BumpPoolBufferStrategyTest, FreeListReuse) {
    std::array<std::size_t, 3> sizes = {100, 200, 300};
    ftl::allocator::BumpPoolBufferStrategy<3> strategy(*bump_allocator_, sizes);
    
    std::vector<ftl::Buffer*> buffers;
    
    // Allocate multiple buffers of different sizes
    for (int i = 0; i < 3; ++i) {
        ftl::Buffer* buf1 = strategy.allocate(50);   // Uses 100-byte class
        ftl::Buffer* buf2 = strategy.allocate(150);  // Uses 200-byte class
        ftl::Buffer* buf3 = strategy.allocate(250);  // Uses 300-byte class
        
        ASSERT_NE(buf1, nullptr);
        ASSERT_NE(buf2, nullptr);
        ASSERT_NE(buf3, nullptr);
        
        buffers.push_back(buf1);
        buffers.push_back(buf2);
        buffers.push_back(buf3);
    }
    
    // Deallocate all (should go to respective free lists)
    for (ftl::Buffer* buf : buffers) {
        strategy.deallocate(buf);
    }
    
    // Allocate again - should reuse from free lists
    for (int i = 0; i < 3; ++i) {
        ftl::Buffer* buf1 = strategy.allocate(50);
        ftl::Buffer* buf2 = strategy.allocate(150);
        ftl::Buffer* buf3 = strategy.allocate(250);
        
        ASSERT_NE(buf1, nullptr);
        ASSERT_NE(buf2, nullptr);
        ASSERT_NE(buf3, nullptr);
        
        // Should get buffers from free list (in LIFO order)
        bool found1 = std::find(buffers.begin(), buffers.end(), buf1) != buffers.end();
        bool found2 = std::find(buffers.begin(), buffers.end(), buf2) != buffers.end();
        bool found3 = std::find(buffers.begin(), buffers.end(), buf3) != buffers.end();
        
        EXPECT_TRUE(found1) << "Buffer not reused from free list";
        EXPECT_TRUE(found2) << "Buffer not reused from free list";
        EXPECT_TRUE(found3) << "Buffer not reused from free list";
        
        strategy.deallocate(buf1);
        strategy.deallocate(buf2);
        strategy.deallocate(buf3);
    }
}

// Test size class selection
TEST_F(BumpPoolBufferStrategyTest, SizeClassSelection) {
    std::array<std::size_t, 4> sizes = {64, 128, 256, 512};
    ftl::allocator::BumpPoolBufferStrategy<4> strategy(*bump_allocator_, sizes);
    
    struct TestCase {
        std::size_t request;
        std::size_t expected;
    };
    
    TestCase cases[] = {
        {1, 64},
        {64, 64},
        {65, 128},
        {128, 128},
        {129, 256},
        {256, 256},
        {257, 512},
        {512, 512},
    };
    
    for (const auto& tc : cases) {
        ftl::Buffer* buf = strategy.allocate(tc.request);
        ASSERT_NE(buf, nullptr) << "Failed to allocate " << tc.request << " bytes";
        EXPECT_EQ(buf->size(), tc.expected) 
            << "Request: " << tc.request << " Expected: " << tc.expected;
        strategy.deallocate(buf);
    }
}

// Test allocation too large
TEST_F(BumpPoolBufferStrategyTest, AllocationTooLarge) {
    std::array<std::size_t, 2> sizes = {64, 128};
    ftl::allocator::BumpPoolBufferStrategy<2> strategy(*bump_allocator_, sizes);
    
    // Request larger than any size class
    ftl::Buffer* buf = strategy.allocate(256);
    EXPECT_EQ(buf, nullptr);
}

// Test multiple allocations with patterns
TEST_F(BumpPoolBufferStrategyTest, MultipleAllocationsWithPatterns) {
    std::array<std::size_t, 3> sizes = {128, 256, 512};
    ftl::allocator::BumpPoolBufferStrategy<3> strategy(*bump_allocator_, sizes);
    
    std::vector<ftl::Buffer*> buffers;
    
    // Allocate multiple buffers and write patterns
    for (int i = 0; i < 10; ++i) {
        std::size_t req_size = static_cast<std::size_t>(50 + i * 40);
        ftl::Buffer* buf = strategy.allocate(req_size);
        ASSERT_NE(buf, nullptr) << "Failed allocation " << i;
        buffers.push_back(buf);
        
        // Write pattern
        std::memset(buf->front(), static_cast<uint8_t>(i), buf->size());
    }
    
    // Verify patterns
    for (size_t i = 0; i < buffers.size(); ++i) {
        ftl::Buffer* buf = buffers[i];
        uint8_t* data = buf->front();
        for (size_t j = 0; j < buf->size(); ++j) {
            EXPECT_EQ(data[j], static_cast<uint8_t>(i))
                << "Buffer " << i << " corrupted at byte " << j;
        }
    }
    
    // Deallocate all
    for (ftl::Buffer* buf : buffers) {
        strategy.deallocate(buf);
    }
}

// Test null pointer handling
TEST_F(BumpPoolBufferStrategyTest, NullPointerHandling) {
    std::array<std::size_t, 1> sizes = {64};
    ftl::allocator::BumpPoolBufferStrategy<1> strategy(*bump_allocator_, sizes);
    
    // Deallocating nullptr should be safe
    strategy.deallocate(nullptr);
}

// Test with custom alignment
TEST_F(BumpPoolBufferStrategyTest, CustomAlignment) {
    constexpr std::size_t CACHE_LINE_SIZE = 64;
    std::array<std::size_t, 3> sizes = {128, 256, 512};
    ftl::allocator::BumpPoolBufferStrategy<3> strategy(*bump_allocator_, sizes, CACHE_LINE_SIZE);
    
    // Allocate multiple buffers and verify alignment
    for (int i = 0; i < 5; ++i) {
        ftl::Buffer* buf = strategy.allocate(static_cast<std::size_t>(100 + i * 50));
        ASSERT_NE(buf, nullptr) << "Failed allocation " << i;
        
        // Verify data is cache-line aligned
        auto addr = reinterpret_cast<std::uintptr_t>(buf->front());
        EXPECT_EQ(addr % CACHE_LINE_SIZE, 0) 
            << "Buffer " << i << " not aligned to " << CACHE_LINE_SIZE << " bytes";
        
        strategy.deallocate(buf);
    }
}

// Test exhaustion and recovery
TEST_F(BumpPoolBufferStrategyTest, ExhaustionAndRecovery) {
    // Use a small arena to test exhaustion
    alignas(64) uint8_t small_arena[1024];
    ftl::BumpAllocator small_bump(small_arena, sizeof(small_arena));
    
    std::array<std::size_t, 1> sizes = {64};
    ftl::allocator::BumpPoolBufferStrategy<1> strategy(small_bump, sizes);
    
    std::vector<ftl::Buffer*> buffers;
    
    // Allocate until exhausted
    while (true) {
        ftl::Buffer* buf = strategy.allocate(50);
        if (!buf) break;
        buffers.push_back(buf);
    }
    
    EXPECT_GT(buffers.size(), 0u);
    size_t max_allocs = buffers.size();
    
    // Should not be able to allocate more
    EXPECT_EQ(strategy.allocate(50), nullptr);
    
    // Free all
    for (ftl::Buffer* buf : buffers) {
        strategy.deallocate(buf);
    }
    
    // Should be able to allocate again (from free list)
    for (size_t i = 0; i < max_allocs; ++i) {
        ftl::Buffer* buf = strategy.allocate(50);
        ASSERT_NE(buf, nullptr) << "Failed to reallocate from free list";
        buffers[i] = buf;
    }
    
    // Cleanup
    for (ftl::Buffer* buf : buffers) {
        strategy.deallocate(buf);
    }
}

// Test with sizes in non-ascending order
TEST_F(BumpPoolBufferStrategyTest, NonAscendingOrder) {
    // Provide sizes in random order
    std::array<std::size_t, 4> sizes = {256, 64, 512, 128};
    ftl::allocator::BumpPoolBufferStrategy<4> strategy(*bump_allocator_, sizes);
    
    // Should still work correctly (base class sorts them)
    ftl::Buffer* buf1 = strategy.allocate(50);
    ASSERT_NE(buf1, nullptr);
    EXPECT_EQ(buf1->size(), 64);  // Smallest size
    strategy.deallocate(buf1);
    
    ftl::Buffer* buf2 = strategy.allocate(100);
    ASSERT_NE(buf2, nullptr);
    EXPECT_EQ(buf2->size(), 128);
    strategy.deallocate(buf2);
    
    ftl::Buffer* buf3 = strategy.allocate(200);
    ASSERT_NE(buf3, nullptr);
    EXPECT_EQ(buf3->size(), 256);
    strategy.deallocate(buf3);
    
    ftl::Buffer* buf4 = strategy.allocate(300);
    ASSERT_NE(buf4, nullptr);
    EXPECT_EQ(buf4->size(), 512);
    strategy.deallocate(buf4);
}