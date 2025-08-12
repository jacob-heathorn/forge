#include "ftl/allocator/malloc_buffer_strategy.hpp"
#include "gtest/gtest.h"
#include <cstring>
#include <vector>

class MallocBufferStrategyTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test basic allocation and deallocation
TEST_F(MallocBufferStrategyTest, BasicAllocation) {
    std::array<std::size_t, 3> sizes = {64, 128, 256};
    ftl::allocator::MallocBufferStrategy<3> strategy(sizes);
    
    // Allocate a small buffer
    ftl::Buffer* buf1 = strategy.allocate(32);
    ASSERT_NE(buf1, nullptr);
    EXPECT_EQ(buf1->size(), 32);  // Buffer reports requested size
    EXPECT_NE(buf1->front(), nullptr);
    
    // Write to buffer to verify it's valid
    std::memset(buf1->front(), 0xAA, buf1->size());
    
    // Deallocate
    strategy.deallocate(buf1);
}

// Test size class selection
TEST_F(MallocBufferStrategyTest, SizeClassSelection) {
    std::array<std::size_t, 4> sizes = {64, 128, 256, 512};
    ftl::allocator::MallocBufferStrategy<4> strategy(sizes);
    
    // Test various sizes
    struct TestCase {
        std::size_t request;
    };
    
    TestCase cases[] = {
        {1},
        {64},
        {65},
        {128},
        {129},
        {256},
        {257},
        {512},
    };
    
    for (const auto& tc : cases) {
        ftl::Buffer* buf = strategy.allocate(tc.request);
        ASSERT_NE(buf, nullptr) << "Failed to allocate " << tc.request << " bytes";
        EXPECT_EQ(buf->size(), tc.request) 
            << "Buffer size should equal requested size: " << tc.request;
        // Verify we can write to the entire requested size
        std::memset(buf->front(), 0xFF, buf->size());
        strategy.deallocate(buf);
    }
}

// Test allocation too large
TEST_F(MallocBufferStrategyTest, AllocationTooLarge) {
    std::array<std::size_t, 2> sizes = {64, 128};
    ftl::allocator::MallocBufferStrategy<2> strategy(sizes);
    
    // Request larger than any size class
    ftl::Buffer* buf = strategy.allocate(256);
    EXPECT_EQ(buf, nullptr);
}

// Test multiple allocations
TEST_F(MallocBufferStrategyTest, MultipleAllocations) {
    std::array<std::size_t, 3> sizes = {100, 200, 300};
    ftl::allocator::MallocBufferStrategy<3> strategy(sizes);
    
    std::vector<ftl::Buffer*> buffers;
    
    // Allocate multiple buffers
    for (int i = 0; i < 10; ++i) {
        std::size_t req_size = static_cast<std::size_t>((i % 3) * 100 + 50);  // 50, 150, 250, 50, ...
        ftl::Buffer* buf = strategy.allocate(req_size);
        ASSERT_NE(buf, nullptr) << "Failed allocation " << i;
        buffers.push_back(buf);
        
        // Write pattern to verify memory
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
TEST_F(MallocBufferStrategyTest, NullPointerHandling) {
    std::array<std::size_t, 1> sizes = {64};
    ftl::allocator::MallocBufferStrategy<1> strategy(sizes);
    
    // Deallocating nullptr should be safe
    strategy.deallocate(nullptr);
}

// Test with sizes provided in non-ascending order
TEST_F(MallocBufferStrategyTest, NonAscendingOrder) {
    // Provide sizes in random order
    std::array<std::size_t, 4> sizes = {256, 64, 512, 128};
    ftl::allocator::MallocBufferStrategy<4> strategy(sizes);
    
    // Should still work correctly (base class sorts them)
    ftl::Buffer* buf1 = strategy.allocate(50);
    ASSERT_NE(buf1, nullptr);
    EXPECT_EQ(buf1->size(), 50);  // Buffer reports requested size
    strategy.deallocate(buf1);
    
    ftl::Buffer* buf2 = strategy.allocate(100);
    ASSERT_NE(buf2, nullptr);
    EXPECT_EQ(buf2->size(), 100);
    strategy.deallocate(buf2);
    
    ftl::Buffer* buf3 = strategy.allocate(200);
    ASSERT_NE(buf3, nullptr);
    EXPECT_EQ(buf3->size(), 200);
    strategy.deallocate(buf3);
    
    ftl::Buffer* buf4 = strategy.allocate(300);
    ASSERT_NE(buf4, nullptr);
    EXPECT_EQ(buf4->size(), 300);
    strategy.deallocate(buf4);
}

// Test the buffer contents can be modified
TEST_F(MallocBufferStrategyTest, BufferContentsModifiable) {
    std::array<std::size_t, 1> sizes = {256};
    ftl::allocator::MallocBufferStrategy<1> strategy(sizes);
    
    ftl::Buffer* buf = strategy.allocate(100);
    ASSERT_NE(buf, nullptr);
    
    // Write sequential values
    uint8_t* data = buf->front();
    for (size_t i = 0; i < buf->size(); ++i) {
        data[i] = static_cast<uint8_t>(i);
    }
    
    // Verify values
    for (size_t i = 0; i < buf->size(); ++i) {
        EXPECT_EQ(data[i], static_cast<uint8_t>(i));
    }
    
    // Use Buffer's back() method
    EXPECT_EQ(buf->back(), data + buf->size());
    
    strategy.deallocate(buf);
}

// Test exact size allocation
TEST_F(MallocBufferStrategyTest, ExactSizeAllocation) {
    std::array<std::size_t, 3> sizes = {64, 128, 256};
    ftl::allocator::MallocBufferStrategy<3> strategy(sizes);
    
    // Request exact sizes
    ftl::Buffer* buf64 = strategy.allocate(64);
    ASSERT_NE(buf64, nullptr);
    EXPECT_EQ(buf64->size(), 64);
    
    ftl::Buffer* buf128 = strategy.allocate(128);
    ASSERT_NE(buf128, nullptr);
    EXPECT_EQ(buf128->size(), 128);
    
    ftl::Buffer* buf256 = strategy.allocate(256);
    ASSERT_NE(buf256, nullptr);
    EXPECT_EQ(buf256->size(), 256);
    
    strategy.deallocate(buf64);
    strategy.deallocate(buf128);
    strategy.deallocate(buf256);
}

// Test with custom alignment
TEST_F(MallocBufferStrategyTest, CustomAlignment) {
    constexpr std::size_t CACHE_LINE_SIZE = 64;
    std::array<std::size_t, 3> sizes = {128, 256, 512};
    ftl::allocator::MallocBufferStrategy<3> strategy(sizes, CACHE_LINE_SIZE);
    
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