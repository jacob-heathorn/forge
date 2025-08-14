#include "ftl/allocator/malloc_buffer_strategy.hpp"
#include "ftl/allocator/buffer_allocator.hpp"
#include "gtest/gtest.h"
#include <cstring>
#include <vector>
#include <memory>

class MallocBufferStrategyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test single strategy allocation and deallocation
TEST_F(MallocBufferStrategyTest, SingleStrategyAllocation) {
    ftl::allocator::MallocBufferStrategy strategy(256);
    
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
    
    // Deallocate
    strategy.deallocate(buf1);
    strategy.deallocate(buf2);
}

// Test multiple strategies with BufferAllocator
TEST_F(MallocBufferStrategyTest, MultipleStrategiesWithAllocator) {
    // Create individual strategies for each size
    auto strategy64 = std::make_unique<ftl::allocator::MallocBufferStrategy>(64);
    auto strategy128 = std::make_unique<ftl::allocator::MallocBufferStrategy>(128);
    auto strategy256 = std::make_unique<ftl::allocator::MallocBufferStrategy>(256);
    
    // Create allocator with array of strategy pointers
    std::array<ftl::allocator::IBufferStrategy*, 3> strategies = {
        strategy64.get(),
        strategy128.get(),
        strategy256.get()
    };
    
    ftl::allocator::BufferAllocator allocator(strategies);
    
    // Test various allocation sizes
    struct TestCase {
        std::size_t request;
        bool should_succeed;
    };
    
    TestCase cases[] = {
        {32, true},   // Should use 64-byte strategy
        {64, true},   // Should use 64-byte strategy
        {65, true},   // Should use 128-byte strategy
        {128, true},  // Should use 128-byte strategy
        {129, true},  // Should use 256-byte strategy
        {256, true},  // Should use 256-byte strategy
        {257, false}, // Should fail - too large
    };
    
    std::vector<ftl::Buffer*> allocated;
    
    for (const auto& tc : cases) {
        ftl::Buffer* buf = allocator.allocate(tc.request);
        if (tc.should_succeed) {
            ASSERT_NE(buf, nullptr) << "Failed to allocate " << tc.request << " bytes";
            EXPECT_EQ(buf->size(), tc.request);
            // Write pattern to verify memory is valid
            std::memset(buf->front(), static_cast<uint8_t>(tc.request & 0xFF), buf->size());
            allocated.push_back(buf);
        } else {
            EXPECT_EQ(buf, nullptr) << "Should not have allocated " << tc.request << " bytes";
        }
    }
    
    // Verify patterns are intact
    for (ftl::Buffer* buf : allocated) {
        uint8_t expected = static_cast<uint8_t>(buf->size() & 0xFF);
        uint8_t* data = buf->front();
        for (size_t i = 0; i < buf->size(); ++i) {
            EXPECT_EQ(data[i], expected) << "Data corruption at byte " << i;
        }
    }
    
    // Deallocate all
    for (ftl::Buffer* buf : allocated) {
        allocator.deallocate(buf);
    }
}

// Test null pointer handling
TEST_F(MallocBufferStrategyTest, NullPointerHandling) {
    ftl::allocator::MallocBufferStrategy strategy(64);
    
    // Deallocating nullptr should be safe
    strategy.deallocate(nullptr);
}

// Test with custom alignment
TEST_F(MallocBufferStrategyTest, CustomAlignment) {
    constexpr std::size_t CACHE_LINE_SIZE = 64;
    ftl::allocator::MallocBufferStrategy strategy(256, CACHE_LINE_SIZE);
    
    EXPECT_EQ(strategy.alignment(), CACHE_LINE_SIZE);
    
    // Allocate and verify alignment
    ftl::Buffer* buf = strategy.allocate(200);
    ASSERT_NE(buf, nullptr);
    
    auto addr = reinterpret_cast<std::uintptr_t>(buf->front());
    EXPECT_EQ(addr % CACHE_LINE_SIZE, 0) << "Buffer not aligned to " << CACHE_LINE_SIZE << " bytes";
    
    strategy.deallocate(buf);
}

// Test mixing different strategy types
TEST_F(MallocBufferStrategyTest, MixedStrategySizes) {
    // Create strategies with different sizes
    auto small = std::make_unique<ftl::allocator::MallocBufferStrategy>(128);
    auto medium = std::make_unique<ftl::allocator::MallocBufferStrategy>(512);
    auto large = std::make_unique<ftl::allocator::MallocBufferStrategy>(2048);
    
    std::array<ftl::allocator::IBufferStrategy*, 3> strategies = {
        small.get(),
        medium.get(),
        large.get()
    };
    
    ftl::allocator::BufferAllocator allocator(strategies);
    
    // Allocate buffers of varying sizes
    std::vector<ftl::Buffer*> buffers;
    
    for (int i = 0; i < 10; ++i) {
        std::size_t size = static_cast<std::size_t>(100 + i * 150);
        ftl::Buffer* buf = allocator.allocate(size);
        
        if (size <= 2048) {
            ASSERT_NE(buf, nullptr) << "Failed to allocate " << size << " bytes";
            EXPECT_EQ(buf->size(), size);
            buffers.push_back(buf);
            
            // Write test pattern
            std::memset(buf->front(), static_cast<uint8_t>(i), buf->size());
        } else {
            EXPECT_EQ(buf, nullptr) << "Should not allocate " << size << " bytes";
        }
    }
    
    // Verify and deallocate
    for (size_t i = 0; i < buffers.size(); ++i) {
        ftl::Buffer* buf = buffers[i];
        uint8_t* data = buf->front();
        uint8_t expected = static_cast<uint8_t>(i);
        
        for (size_t j = 0; j < buf->size(); ++j) {
            EXPECT_EQ(data[j], expected) << "Data corruption in buffer " << i << " at byte " << j;
        }
        
        allocator.deallocate(buf);
    }
}

// Test allocator with unsorted strategies (should auto-sort)
TEST_F(MallocBufferStrategyTest, UnsortedStrategies) {
    // Create strategies in non-ascending order
    auto large = std::make_unique<ftl::allocator::MallocBufferStrategy>(512);
    auto small = std::make_unique<ftl::allocator::MallocBufferStrategy>(64);
    auto medium = std::make_unique<ftl::allocator::MallocBufferStrategy>(256);
    
    std::array<ftl::allocator::IBufferStrategy*, 3> strategies = {
        large.get(),  // 512
        small.get(),  // 64
        medium.get()  // 256
    };
    
    ftl::allocator::BufferAllocator allocator(strategies);
    
    // Verify sizes are sorted internally
    EXPECT_EQ(allocator.size(0), 64);
    EXPECT_EQ(allocator.size(1), 256);
    EXPECT_EQ(allocator.size(2), 512);
    
    // Test allocation still works correctly
    ftl::Buffer* buf1 = allocator.allocate(50);
    ASSERT_NE(buf1, nullptr);
    EXPECT_EQ(buf1->size(), 50);
    
    ftl::Buffer* buf2 = allocator.allocate(200);
    ASSERT_NE(buf2, nullptr);
    EXPECT_EQ(buf2->size(), 200);
    
    ftl::Buffer* buf3 = allocator.allocate(400);
    ASSERT_NE(buf3, nullptr);
    EXPECT_EQ(buf3->size(), 400);
    
    allocator.deallocate(buf1);
    allocator.deallocate(buf2);
    allocator.deallocate(buf3);
}