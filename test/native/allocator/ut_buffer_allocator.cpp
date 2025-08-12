#include "ftl/allocator/buffer_allocator.hpp"
#include "ftl/allocator/malloc_buffer_strategy.hpp"
#include "gtest/gtest.h"
#include <cstring>
#include <memory>
#include <vector>

class BufferAllocatorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test basic allocation with MallocBufferStrategy
TEST_F(BufferAllocatorTest, BasicAllocation) {
    std::array<std::size_t, 3> sizes = {64, 128, 256};
    ftl::allocator::MallocBufferStrategy<3> strategy(sizes);
    ftl::allocator::BufferAllocator<3> allocator(strategy);
    
    // Allocate a small buffer
    ftl::Buffer* buf1 = allocator.allocate(32);
    ASSERT_NE(buf1, nullptr);
    EXPECT_EQ(buf1->size(), 64);
    
    // Allocate a medium buffer
    ftl::Buffer* buf2 = allocator.allocate(100);
    ASSERT_NE(buf2, nullptr);
    EXPECT_EQ(buf2->size(), 128);
    
    // Allocate a larger buffer
    ftl::Buffer* buf3 = allocator.allocate(200);
    ASSERT_NE(buf3, nullptr);
    EXPECT_EQ(buf3->size(), 256);
    
    // Write to buffers to verify they're valid
    std::memset(buf1->front(), 0xAA, buf1->size());
    std::memset(buf2->front(), 0xBB, buf2->size());
    std::memset(buf3->front(), 0xCC, buf3->size());
    
    // Deallocate
    allocator.deallocate(buf1);
    allocator.deallocate(buf2);
    allocator.deallocate(buf3);
}

// Test sizes and alignment accessors
TEST_F(BufferAllocatorTest, Accessors) {
    std::array<std::size_t, 4> sizes = {100, 200, 300, 400};
    constexpr std::size_t alignment = 64;
    ftl::allocator::MallocBufferStrategy<4> strategy(sizes, alignment);
    ftl::allocator::BufferAllocator<4> allocator(strategy);
    
    // Test num_slots
    EXPECT_EQ(allocator.num_slots(), 4);
    
    // Test sizes (should be sorted)
    const auto& sorted_sizes = allocator.sizes();
    EXPECT_EQ(sorted_sizes[0], 100);
    EXPECT_EQ(sorted_sizes[1], 200);
    EXPECT_EQ(sorted_sizes[2], 300);
    EXPECT_EQ(sorted_sizes[3], 400);
    
    // Test individual size accessor
    EXPECT_EQ(allocator.size(0), 100);
    EXPECT_EQ(allocator.size(1), 200);
    EXPECT_EQ(allocator.size(2), 300);
    EXPECT_EQ(allocator.size(3), 400);
    EXPECT_EQ(allocator.size(4), 0);  // Out of bounds
    
    // Test alignment
    EXPECT_EQ(allocator.alignment(), alignment);
}

// Test null pointer handling
TEST_F(BufferAllocatorTest, NullPointerHandling) {
    std::array<std::size_t, 1> sizes = {64};
    ftl::allocator::MallocBufferStrategy<1> strategy(sizes);
    ftl::allocator::BufferAllocator<1> allocator(strategy);
    
    // Deallocating nullptr should be safe
    allocator.deallocate(nullptr);
}

// Test multiple allocations and deallocations
TEST_F(BufferAllocatorTest, MultipleAllocations) {
    std::array<std::size_t, 3> sizes = {128, 256, 512};
    ftl::allocator::MallocBufferStrategy<3> strategy(sizes);
    ftl::allocator::BufferAllocator<3> allocator(strategy);
    
    std::vector<ftl::Buffer*> buffers;
    
    // Allocate multiple buffers
    for (int i = 0; i < 10; ++i) {
        std::size_t req_size = static_cast<std::size_t>(50 + i * 30);
        ftl::Buffer* buf = allocator.allocate(req_size);
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
        allocator.deallocate(buf);
    }
}

// Test allocation too large
TEST_F(BufferAllocatorTest, AllocationTooLarge) {
    std::array<std::size_t, 2> sizes = {64, 128};
    ftl::allocator::MallocBufferStrategy<2> strategy(sizes);
    ftl::allocator::BufferAllocator<2> allocator(strategy);
    
    // Request larger than any size class
    ftl::Buffer* buf = allocator.allocate(256);
    EXPECT_EQ(buf, nullptr);
}

// Test with custom alignment
TEST_F(BufferAllocatorTest, CustomAlignment) {
    constexpr std::size_t CACHE_LINE_SIZE = 64;
    std::array<std::size_t, 3> sizes = {128, 256, 512};
    ftl::allocator::MallocBufferStrategy<3> strategy(sizes, CACHE_LINE_SIZE);
    ftl::allocator::BufferAllocator<3> allocator(strategy);
    
    EXPECT_EQ(allocator.alignment(), CACHE_LINE_SIZE);
    
    // Allocate and verify alignment
    ftl::Buffer* buf = allocator.allocate(100);
    ASSERT_NE(buf, nullptr);
    
    auto addr = reinterpret_cast<std::uintptr_t>(buf->front());
    EXPECT_EQ(addr % CACHE_LINE_SIZE, 0) << "Buffer not aligned";
    
    allocator.deallocate(buf);
}

// Test with sizes in non-ascending order (should be sorted by strategy)
TEST_F(BufferAllocatorTest, NonAscendingOrder) {
    // Provide sizes in random order
    std::array<std::size_t, 4> sizes = {256, 64, 512, 128};
    ftl::allocator::MallocBufferStrategy<4> strategy(sizes);
    ftl::allocator::BufferAllocator<4> allocator(strategy);
    
    // Verify sizes are sorted
    const auto& sorted_sizes = allocator.sizes();
    EXPECT_EQ(sorted_sizes[0], 64);
    EXPECT_EQ(sorted_sizes[1], 128);
    EXPECT_EQ(sorted_sizes[2], 256);
    EXPECT_EQ(sorted_sizes[3], 512);
    
    // Test allocation still works correctly
    ftl::Buffer* buf1 = allocator.allocate(50);
    ASSERT_NE(buf1, nullptr);
    EXPECT_EQ(buf1->size(), 64);
    
    ftl::Buffer* buf2 = allocator.allocate(200);
    ASSERT_NE(buf2, nullptr);
    EXPECT_EQ(buf2->size(), 256);
    
    allocator.deallocate(buf1);
    allocator.deallocate(buf2);
}

