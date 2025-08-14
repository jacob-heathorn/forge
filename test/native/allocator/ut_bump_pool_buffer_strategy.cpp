#include "ftl/allocator/bump_pool_buffer_strategy.hpp"
#include "ftl/allocator/bump_allocator.hpp"
#include "ftl/allocator/buffer_allocator.hpp"
#include "ftl/allocator/malloc_buffer_strategy.hpp"
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

// Test single strategy allocation
TEST_F(BumpPoolBufferStrategyTest, SingleStrategyAllocation) {
    ftl::allocator::BumpPoolBufferStrategy strategy(*bump_allocator_, 256);
    
    // Allocate within size
    ftl::Buffer* buf1 = strategy.allocate(100);
    ASSERT_NE(buf1, nullptr);
    EXPECT_EQ(buf1->size(), 100);
    EXPECT_NE(buf1->front(), nullptr);
    
    // Write to buffer to verify it's valid
    std::memset(buf1->front(), 0xAA, buf1->size());
    
    // Allocate at exact size
    ftl::Buffer* buf2 = strategy.allocate(256);
    ASSERT_NE(buf2, nullptr);
    EXPECT_EQ(buf2->size(), 256);
    
    // Allocate beyond size should fail
    ftl::Buffer* buf3 = strategy.allocate(257);
    EXPECT_EQ(buf3, nullptr);
    
    // Deallocate (goes to free list)
    strategy.deallocate(buf1);
    strategy.deallocate(buf2);
    
    // Allocate again - should reuse from free list
    ftl::Buffer* buf4 = strategy.allocate(100);
    ASSERT_NE(buf4, nullptr);
    EXPECT_EQ(buf4, buf2);  // Should get buf2 back (LIFO free list)
}

// Test free list reuse with multiple strategies
TEST_F(BumpPoolBufferStrategyTest, FreeListReuseWithAllocator) {
    // Create individual strategies
    auto strategy100 = std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*bump_allocator_, 100);
    auto strategy200 = std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*bump_allocator_, 200);
    auto strategy300 = std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*bump_allocator_, 300);
    
    std::array<ftl::allocator::IBufferStrategy*, 3> strategies = {
        strategy100.get(),
        strategy200.get(),
        strategy300.get()
    };
    
    ftl::allocator::BufferAllocator allocator(strategies);
    
    std::vector<ftl::Buffer*> buffers;
    
    // Allocate multiple buffers of different sizes
    for (int i = 0; i < 3; ++i) {
        ftl::Buffer* buf1 = allocator.allocate(50);   // Uses 100-byte strategy
        ftl::Buffer* buf2 = allocator.allocate(150);  // Uses 200-byte strategy
        ftl::Buffer* buf3 = allocator.allocate(250);  // Uses 300-byte strategy
        
        ASSERT_NE(buf1, nullptr);
        ASSERT_NE(buf2, nullptr);
        ASSERT_NE(buf3, nullptr);
        
        buffers.push_back(buf1);
        buffers.push_back(buf2);
        buffers.push_back(buf3);
    }
    
    // Deallocate all (should go to respective free lists)
    for (ftl::Buffer* buf : buffers) {
        allocator.deallocate(buf);
    }
    
    // Allocate again - should reuse from free lists
    for (int i = 0; i < 3; ++i) {
        ftl::Buffer* buf1 = allocator.allocate(50);
        ftl::Buffer* buf2 = allocator.allocate(150);
        ftl::Buffer* buf3 = allocator.allocate(250);
        
        ASSERT_NE(buf1, nullptr);
        ASSERT_NE(buf2, nullptr);
        ASSERT_NE(buf3, nullptr);
        
        // Should get buffers from free list
        bool found1 = std::find(buffers.begin(), buffers.end(), buf1) != buffers.end();
        bool found2 = std::find(buffers.begin(), buffers.end(), buf2) != buffers.end();
        bool found3 = std::find(buffers.begin(), buffers.end(), buf3) != buffers.end();
        
        EXPECT_TRUE(found1) << "Buffer not reused from free list";
        EXPECT_TRUE(found2) << "Buffer not reused from free list";
        EXPECT_TRUE(found3) << "Buffer not reused from free list";
        
        allocator.deallocate(buf1);
        allocator.deallocate(buf2);
        allocator.deallocate(buf3);
    }
}

// Test size class selection with allocator
TEST_F(BumpPoolBufferStrategyTest, SizeClassSelection) {
    auto strategy64 = std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*bump_allocator_, 64);
    auto strategy128 = std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*bump_allocator_, 128);
    auto strategy256 = std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*bump_allocator_, 256);
    auto strategy512 = std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*bump_allocator_, 512);
    
    std::array<ftl::allocator::IBufferStrategy*, 4> strategies = {
        strategy64.get(),
        strategy128.get(),
        strategy256.get(),
        strategy512.get()
    };
    
    ftl::allocator::BufferAllocator allocator(strategies);
    
    struct TestCase {
        std::size_t request;
        bool should_succeed;
    };
    
    TestCase cases[] = {
        {1, true},
        {64, true},
        {65, true},
        {128, true},
        {129, true},
        {256, true},
        {257, true},
        {512, true},
        {513, false},  // Too large
    };
    
    for (const auto& tc : cases) {
        ftl::Buffer* buf = allocator.allocate(tc.request);
        if (tc.should_succeed) {
            ASSERT_NE(buf, nullptr) << "Failed to allocate " << tc.request << " bytes";
            EXPECT_EQ(buf->size(), tc.request) 
                << "Buffer size should equal requested size: " << tc.request;
            // Verify we can write to the entire requested size
            std::memset(buf->front(), 0xFF, buf->size());
            allocator.deallocate(buf);
        } else {
            EXPECT_EQ(buf, nullptr) << "Should not allocate " << tc.request << " bytes";
        }
    }
}

// Test null pointer handling
TEST_F(BumpPoolBufferStrategyTest, NullPointerHandling) {
    ftl::allocator::BumpPoolBufferStrategy strategy(*bump_allocator_, 64);
    
    // Deallocating nullptr should be safe
    strategy.deallocate(nullptr);
}

// Test with custom alignment
TEST_F(BumpPoolBufferStrategyTest, CustomAlignment) {
    constexpr std::size_t CACHE_LINE_SIZE = 64;
    
    auto strategy128 = std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*bump_allocator_, 128, CACHE_LINE_SIZE);
    auto strategy256 = std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*bump_allocator_, 256, CACHE_LINE_SIZE);
    auto strategy512 = std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*bump_allocator_, 512, CACHE_LINE_SIZE);
    
    std::array<ftl::allocator::IBufferStrategy*, 3> strategies = {
        strategy128.get(),
        strategy256.get(),
        strategy512.get()
    };
    
    ftl::allocator::BufferAllocator allocator(strategies);
    
    // Allocate multiple buffers and verify alignment
    for (int i = 0; i < 5; ++i) {
        ftl::Buffer* buf = allocator.allocate(static_cast<std::size_t>(100 + i * 50));
        ASSERT_NE(buf, nullptr) << "Failed allocation " << i;
        
        // Verify data is cache-line aligned
        auto addr = reinterpret_cast<std::uintptr_t>(buf->front());
        EXPECT_EQ(addr % CACHE_LINE_SIZE, 0) 
            << "Buffer " << i << " not aligned to " << CACHE_LINE_SIZE << " bytes";
        
        allocator.deallocate(buf);
    }
}

// Test exhaustion and recovery
TEST_F(BumpPoolBufferStrategyTest, ExhaustionAndRecovery) {
    // Use a small arena to test exhaustion
    alignas(64) uint8_t small_arena[1024];
    ftl::BumpAllocator small_bump(small_arena, sizeof(small_arena));
    
    ftl::allocator::BumpPoolBufferStrategy strategy(small_bump, 64);
    
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

// Test with mixed strategy types in allocator
TEST_F(BumpPoolBufferStrategyTest, MixedStrategies) {
    // Create a mix of bump pool and malloc strategies
    auto bump128 = std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*bump_allocator_, 128);
    auto bump256 = std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*bump_allocator_, 256);
    auto malloc512 = std::make_unique<ftl::allocator::MallocBufferStrategy>(512);
    auto bump1024 = std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*bump_allocator_, 1024);
    
    std::array<ftl::allocator::IBufferStrategy*, 4> strategies = {
        bump128.get(),
        bump256.get(),
        malloc512.get(),
        bump1024.get()
    };
    
    ftl::allocator::BufferAllocator allocator(strategies);
    
    // Test various allocations
    std::vector<ftl::Buffer*> buffers;
    
    for (int i = 0; i < 10; ++i) {
        std::size_t size = static_cast<std::size_t>(50 + i * 80);
        ftl::Buffer* buf = allocator.allocate(size);
        
        if (size <= 1024) {
            ASSERT_NE(buf, nullptr) << "Failed to allocate " << size << " bytes";
            EXPECT_EQ(buf->size(), size);
            buffers.push_back(buf);
            
            // Write test pattern
            std::memset(buf->front(), static_cast<uint8_t>(i), buf->size());
        } else {
            EXPECT_EQ(buf, nullptr);
        }
    }
    
    // Verify patterns
    for (size_t i = 0; i < buffers.size(); ++i) {
        ftl::Buffer* buf = buffers[i];
        uint8_t* data = buf->front();
        uint8_t expected = static_cast<uint8_t>(i);
        
        for (size_t j = 0; j < buf->size(); ++j) {
            EXPECT_EQ(data[j], expected) 
                << "Buffer " << i << " corrupted at byte " << j;
        }
    }
    
    // Deallocate all
    for (ftl::Buffer* buf : buffers) {
        allocator.deallocate(buf);
    }
}

// Test that BufferNode and data don't overlap even with alignment padding
TEST_F(BumpPoolBufferStrategyTest, NoOverlapBetweenNodeAndData) {
    constexpr std::size_t ALIGN = 64;
    ftl::allocator::BumpPoolBufferStrategy strategy(*bump_allocator_, 256, ALIGN);
    
    ftl::Buffer* buf = strategy.allocate(200);
    ASSERT_NE(buf, nullptr);
    
    // The Buffer is embedded in BufferNode at offset of BufferNode::buffer
    // Calculate where BufferNode starts
    auto buffer_offset = offsetof(ftl::allocator::BumpPoolBufferStrategy::BufferNode, buffer);
    auto node_start = reinterpret_cast<uintptr_t>(buf) - buffer_offset;
    auto node_end = node_start + sizeof(ftl::allocator::BumpPoolBufferStrategy::BufferNode);
    
    // Get buffer data location
    uintptr_t data_start = reinterpret_cast<uintptr_t>(buf->front());
    
    // Verify no overlap
    EXPECT_LE(node_end, data_start) 
        << "BufferNode and buffer data overlap! "
        << "Node ends at 0x" << std::hex << node_end 
        << ", data starts at 0x" << std::hex << data_start;
    
    // Verify data is aligned as requested
    EXPECT_EQ(data_start % ALIGN, 0) << "Data not properly aligned";
    
    strategy.deallocate(buf);
}