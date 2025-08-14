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
    // Create individual strategies
    auto strategy64 = std::make_unique<ftl::allocator::MallocBufferStrategy>(64);
    auto strategy128 = std::make_unique<ftl::allocator::MallocBufferStrategy>(128);
    auto strategy256 = std::make_unique<ftl::allocator::MallocBufferStrategy>(256);
    
    std::array<ftl::allocator::IBufferStrategy*, 3> strategies = {
        strategy64.get(),
        strategy128.get(),
        strategy256.get()
    };
    
    ftl::allocator::BufferAllocator allocator(strategies);
    
    // Allocate a small buffer
    ftl::Buffer* buf1 = allocator.allocate(32);
    ASSERT_NE(buf1, nullptr);
    EXPECT_EQ(buf1->size(), 32);  // Buffer reports requested size
    
    // Allocate a medium buffer
    ftl::Buffer* buf2 = allocator.allocate(100);
    ASSERT_NE(buf2, nullptr);
    EXPECT_EQ(buf2->size(), 100);
    
    // Allocate a larger buffer
    ftl::Buffer* buf3 = allocator.allocate(200);
    ASSERT_NE(buf3, nullptr);
    EXPECT_EQ(buf3->size(), 200);
    
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
    // Create strategies with specific sizes
    auto strategy100 = std::make_unique<ftl::allocator::MallocBufferStrategy>(100, 64);
    auto strategy200 = std::make_unique<ftl::allocator::MallocBufferStrategy>(200, 64);
    auto strategy300 = std::make_unique<ftl::allocator::MallocBufferStrategy>(300, 64);
    auto strategy400 = std::make_unique<ftl::allocator::MallocBufferStrategy>(400, 64);
    
    std::array<ftl::allocator::IBufferStrategy*, 4> strategies = {
        strategy100.get(),
        strategy200.get(),
        strategy300.get(),
        strategy400.get()
    };
    
    ftl::allocator::BufferAllocator allocator(strategies);
    
    // Test num_slots
    EXPECT_EQ(allocator.num_slots(), 4u);
    
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
    
    // Test alignment for each strategy
    EXPECT_EQ(allocator.alignment(0), 64);
    EXPECT_EQ(allocator.alignment(1), 64);
    EXPECT_EQ(allocator.alignment(2), 64);
    EXPECT_EQ(allocator.alignment(3), 64);
}

// Test null pointer handling
TEST_F(BufferAllocatorTest, NullPointerHandling) {
    auto strategy = std::make_unique<ftl::allocator::MallocBufferStrategy>(64);
    std::array<ftl::allocator::IBufferStrategy*, 1> strategies = {strategy.get()};
    ftl::allocator::BufferAllocator allocator(strategies);
    
    // Deallocating nullptr should be safe
    allocator.deallocate(nullptr);
}

// Test multiple allocations and deallocations
TEST_F(BufferAllocatorTest, MultipleAllocations) {
    auto strategy128 = std::make_unique<ftl::allocator::MallocBufferStrategy>(128);
    auto strategy256 = std::make_unique<ftl::allocator::MallocBufferStrategy>(256);
    auto strategy512 = std::make_unique<ftl::allocator::MallocBufferStrategy>(512);
    
    std::array<ftl::allocator::IBufferStrategy*, 3> strategies = {
        strategy128.get(),
        strategy256.get(),
        strategy512.get()
    };
    
    ftl::allocator::BufferAllocator allocator(strategies);
    
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
    auto strategy64 = std::make_unique<ftl::allocator::MallocBufferStrategy>(64);
    auto strategy128 = std::make_unique<ftl::allocator::MallocBufferStrategy>(128);
    
    std::array<ftl::allocator::IBufferStrategy*, 2> strategies = {
        strategy64.get(),
        strategy128.get()
    };
    
    ftl::allocator::BufferAllocator allocator(strategies);
    
    // Request larger than any size class
    ftl::Buffer* buf = allocator.allocate(256);
    EXPECT_EQ(buf, nullptr);
}

// Test with custom alignment
TEST_F(BufferAllocatorTest, CustomAlignment) {
    constexpr std::size_t CACHE_LINE_SIZE = 64;
    
    // Create strategies with custom alignment
    auto strategy128 = std::make_unique<ftl::allocator::MallocBufferStrategy>(128, CACHE_LINE_SIZE);
    auto strategy256 = std::make_unique<ftl::allocator::MallocBufferStrategy>(256, CACHE_LINE_SIZE);
    auto strategy512 = std::make_unique<ftl::allocator::MallocBufferStrategy>(512, CACHE_LINE_SIZE);
    
    std::array<ftl::allocator::IBufferStrategy*, 3> strategies = {
        strategy128.get(),
        strategy256.get(),
        strategy512.get()
    };
    
    ftl::allocator::BufferAllocator allocator(strategies);
    
    // Check alignment of each strategy
    EXPECT_EQ(allocator.alignment(0), CACHE_LINE_SIZE);
    EXPECT_EQ(allocator.alignment(1), CACHE_LINE_SIZE);
    EXPECT_EQ(allocator.alignment(2), CACHE_LINE_SIZE);
    
    // Allocate and verify alignment
    ftl::Buffer* buf = allocator.allocate(100);
    ASSERT_NE(buf, nullptr);
    
    auto addr = reinterpret_cast<std::uintptr_t>(buf->front());
    EXPECT_EQ(addr % CACHE_LINE_SIZE, 0) << "Buffer not aligned";
    
    allocator.deallocate(buf);
}

// Test with sizes in non-ascending order (should be sorted by allocator)
TEST_F(BufferAllocatorTest, NonAscendingOrder) {
    // Create strategies in random order
    auto strategy256 = std::make_unique<ftl::allocator::MallocBufferStrategy>(256);
    auto strategy64 = std::make_unique<ftl::allocator::MallocBufferStrategy>(64);
    auto strategy512 = std::make_unique<ftl::allocator::MallocBufferStrategy>(512);
    auto strategy128 = std::make_unique<ftl::allocator::MallocBufferStrategy>(128);
    
    std::array<ftl::allocator::IBufferStrategy*, 4> strategies = {
        strategy256.get(),
        strategy64.get(),
        strategy512.get(),
        strategy128.get()
    };
    
    ftl::allocator::BufferAllocator allocator(strategies);
    
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