#include "ftl/allocator/buffer_allocator.hpp"
#include "ftl/allocator/malloc_strategy.hpp"
#include "ftl/allocator/bump_pool_strategy.hpp"
#include "ftl/bump_allocator.hpp"
#include "gtest/gtest.h"
#include <memory>
#include <cstring>
#include <vector>

namespace {

// Helper to create malloc strategies for different sizes
std::unique_ptr<ftl::allocator::MallocBlockStrategy> 
make_malloc_strategy(std::size_t size) {
    return std::make_unique<ftl::allocator::MallocBlockStrategy>(size);
}

}  // namespace

class BufferAllocatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup for bump pool tests
        std::memset(arena_, 0, sizeof(arena_));
        bump_allocator_ = std::make_unique<ftl::BumpAllocator>(arena_, sizeof(arena_));
    }
    
    void TearDown() override {
        bump_allocator_.reset();
    }
    
    alignas(64) uint8_t arena_[8192];
    std::unique_ptr<ftl::BumpAllocator> bump_allocator_;
};

// Test basic allocation with malloc strategies
TEST_F(BufferAllocatorTest, BasicAllocationMalloc) {
    // Create strategies for 64, 128, 256 byte buffers
    auto strat64 = make_malloc_strategy(64);
    auto strat128 = make_malloc_strategy(128);
    auto strat256 = make_malloc_strategy(256);
    
    ftl::allocator::BufferAllocator<3> allocator({
        strat64.get(), strat128.get(), strat256.get()
    });
    
    // Allocate a small buffer (should use 64-byte slot)
    auto buf1 = allocator.allocate(32);
    ASSERT_TRUE(buf1);
    EXPECT_EQ(buf1.size, 64);
    EXPECT_EQ(buf1.slot_index, 0);
    
    // Allocate a medium buffer (should use 128-byte slot)
    auto buf2 = allocator.allocate(100);
    ASSERT_TRUE(buf2);
    EXPECT_EQ(buf2.size, 128);
    EXPECT_EQ(buf2.slot_index, 1);
    
    // Allocate a larger buffer (should use 256-byte slot)
    auto buf3 = allocator.allocate(200);
    ASSERT_TRUE(buf3);
    EXPECT_EQ(buf3.size, 256);
    EXPECT_EQ(buf3.slot_index, 2);
    
    // Write to buffers to verify they're valid
    std::memset(buf1.ptr, 0xAA, buf1.size);
    std::memset(buf2.ptr, 0xBB, buf2.size);
    std::memset(buf3.ptr, 0xCC, buf3.size);
    
    // Deallocate
    allocator.deallocate(buf1);
    allocator.deallocate(buf2);
    allocator.deallocate(buf3);
}

// Test size class selection
TEST_F(BufferAllocatorTest, SizeClassSelection) {
    auto strat64 = make_malloc_strategy(64);
    auto strat128 = make_malloc_strategy(128);
    auto strat256 = make_malloc_strategy(256);
    
    ftl::allocator::BufferAllocator<3> allocator({
        strat64.get(), strat128.get(), strat256.get()
    });
    
    // Test exact size boundaries
    auto buf_exact64 = allocator.allocate(64);
    EXPECT_EQ(buf_exact64.size, 64);
    EXPECT_EQ(buf_exact64.slot_index, 0);
    allocator.deallocate(buf_exact64);
    
    auto buf_exact128 = allocator.allocate(128);
    EXPECT_EQ(buf_exact128.size, 128);
    EXPECT_EQ(buf_exact128.slot_index, 1);
    allocator.deallocate(buf_exact128);
    
    auto buf_exact256 = allocator.allocate(256);
    EXPECT_EQ(buf_exact256.size, 256);
    EXPECT_EQ(buf_exact256.slot_index, 2);
    allocator.deallocate(buf_exact256);
    
    // Test just over boundaries
    auto buf_65 = allocator.allocate(65);
    EXPECT_EQ(buf_65.size, 128);
    EXPECT_EQ(buf_65.slot_index, 1);
    allocator.deallocate(buf_65);
    
    auto buf_129 = allocator.allocate(129);
    EXPECT_EQ(buf_129.size, 256);
    EXPECT_EQ(buf_129.slot_index, 2);
    allocator.deallocate(buf_129);
}

// Test allocation too large for any class
TEST_F(BufferAllocatorTest, AllocationTooLarge) {
    auto strat64 = make_malloc_strategy(64);
    auto strat128 = make_malloc_strategy(128);
    
    ftl::allocator::BufferAllocator<2> allocator({
        strat64.get(), strat128.get()
    });
    
    // Request larger than largest class
    auto buf = allocator.allocate(256);
    EXPECT_FALSE(buf);
    EXPECT_EQ(buf.ptr, nullptr);
    EXPECT_EQ(buf.size, 0);
}

// Test with bump pool strategies
TEST_F(BufferAllocatorTest, BumpPoolStrategies) {
    // Create bump pool strategies
    ftl::allocator::BumpPoolBlockStrategy strat64(*bump_allocator_, 64);
    ftl::allocator::BumpPoolBlockStrategy strat128(*bump_allocator_, 128);
    ftl::allocator::BumpPoolBlockStrategy strat256(*bump_allocator_, 256);
    
    ftl::allocator::BufferAllocator<3> allocator({
        &strat64, &strat128, &strat256
    });
    
    // Allocate buffers
    std::vector<ftl::allocator::Buffer> buffers;
    for (int i = 0; i < 10; ++i) {
        std::size_t size = static_cast<std::size_t>((i % 3) * 50 + 30);  // Varies between 30, 80, 130
        auto buf = allocator.allocate(size);
        ASSERT_TRUE(buf) << "Failed to allocate buffer " << i;
        buffers.push_back(buf);
        
        // Write pattern to verify memory
        std::memset(buf.ptr, static_cast<uint8_t>(i), buf.size);
    }
    
    // Verify patterns
    for (size_t i = 0; i < buffers.size(); ++i) {
        auto& buf = buffers[i];
        uint8_t* data = static_cast<uint8_t*>(buf.ptr);
        for (size_t j = 0; j < buf.size; ++j) {
            EXPECT_EQ(data[j], static_cast<uint8_t>(i));
        }
    }
    
    // Deallocate all - should go to free lists
    for (auto& buf : buffers) {
        allocator.deallocate(buf);
    }
    
    // Allocate again - should reuse from free lists
    for (int i = 0; i < 10; ++i) {
        std::size_t size = static_cast<std::size_t>((i % 3) * 50 + 30);
        auto buf = allocator.allocate(size);
        ASSERT_TRUE(buf) << "Failed to reallocate buffer " << i;
        allocator.deallocate(buf);
    }
}

// Test null/invalid buffer handling
TEST_F(BufferAllocatorTest, NullBufferHandling) {
    auto strat64 = make_malloc_strategy(64);
    
    ftl::allocator::BufferAllocator<1> allocator({strat64.get()});
    
    // Deallocating invalid buffer should be safe
    ftl::allocator::Buffer invalid_buf;
    allocator.deallocate(invalid_buf);
    
    // Deallocating buffer with null ptr should be safe
    ftl::allocator::Buffer null_buf{nullptr, 64, 0};
    allocator.deallocate(null_buf);
    
    // Deallocating buffer with out-of-range slot index should be safe
    void* dummy = &dummy;  // Just a non-null pointer
    ftl::allocator::Buffer bad_index{dummy, 64, 999};
    allocator.deallocate(bad_index);  // Should not crash
}

// Test with missing strategy
TEST_F(BufferAllocatorTest, MissingStrategy) {
    auto strat64 = make_malloc_strategy(64);
    
    // Second strategy is nullptr
    ftl::allocator::BufferAllocator<2> allocator({
        strat64.get(), nullptr
    });
    
    // Allocation for first slot should work
    auto buf1 = allocator.allocate(50);
    ASSERT_TRUE(buf1);
    EXPECT_EQ(buf1.size, 64);
    allocator.deallocate(buf1);
    
    // Allocation that would need second slot should fail
    auto buf2 = allocator.allocate(100);
    EXPECT_FALSE(buf2);
}

// Test slot functions
TEST_F(BufferAllocatorTest, SlotFunctions) {
    auto strat64 = make_malloc_strategy(64);
    auto strat128 = make_malloc_strategy(128);
    auto strat256 = make_malloc_strategy(256);
    auto strat512 = make_malloc_strategy(512);
    
    ftl::allocator::BufferAllocator<4> allocator({
        strat64.get(), strat128.get(), strat256.get(), strat512.get()
    });
    
    EXPECT_EQ(allocator.num_slots(), 4);
    EXPECT_EQ(allocator.slot_size(0), 64);
    EXPECT_EQ(allocator.slot_size(1), 128);
    EXPECT_EQ(allocator.slot_size(2), 256);
    EXPECT_EQ(allocator.slot_size(3), 512);
    EXPECT_EQ(allocator.slot_size(4), 0);  // Out of bounds
}

// Test power-of-two configuration
TEST_F(BufferAllocatorTest, PowerOfTwoConfiguration) {
    // Create strategies for power-of-two sizes
    auto strat64 = make_malloc_strategy(64);
    auto strat128 = make_malloc_strategy(128);
    auto strat256 = make_malloc_strategy(256);
    auto strat512 = make_malloc_strategy(512);
    auto strat1024 = make_malloc_strategy(1024);
    auto strat2048 = make_malloc_strategy(2048);
    auto strat4096 = make_malloc_strategy(4096);
    
    ftl::allocator::BufferAllocator<7> allocator({
        strat64.get(), strat128.get(), strat256.get(), strat512.get(),
        strat1024.get(), strat2048.get(), strat4096.get()
    });
    
    // Test various sizes
    struct TestCase {
        std::size_t request;
        std::size_t expected_size;
        std::uint16_t expected_index;
    };
    
    TestCase cases[] = {
        {1, 64, 0},
        {64, 64, 0},
        {65, 128, 1},
        {256, 256, 2},
        {257, 512, 3},
        {1000, 1024, 4},
        {2000, 2048, 5},
        {4000, 4096, 6},
    };
    
    for (const auto& tc : cases) {
        auto buf = allocator.allocate(tc.request);
        ASSERT_TRUE(buf) << "Failed to allocate " << tc.request << " bytes";
        EXPECT_EQ(buf.size, tc.expected_size) 
            << "Request: " << tc.request;
        EXPECT_EQ(buf.slot_index, tc.expected_index)
            << "Request: " << tc.request;
        allocator.deallocate(buf);
    }
    
    // Test too large
    auto too_large = allocator.allocate(5000);
    EXPECT_FALSE(too_large);
}

// Test buffer reuse pattern
TEST_F(BufferAllocatorTest, BufferReusePattern) {
    ftl::allocator::BumpPoolBlockStrategy strat128(*bump_allocator_, 128);
    
    ftl::allocator::BufferAllocator<1> allocator({&strat128});
    
    // Allocate a buffer
    auto buf1 = allocator.allocate(100);
    ASSERT_TRUE(buf1);
    void* ptr1 = buf1.ptr;
    
    // Deallocate it
    allocator.deallocate(buf1);
    
    // Allocate again - should get same memory back (from free list)
    auto buf2 = allocator.allocate(100);
    ASSERT_TRUE(buf2);
    EXPECT_EQ(buf2.ptr, ptr1) << "Should reuse memory from free list";
    
    allocator.deallocate(buf2);
}

// Test with strategies in non-ascending order (automatically sorted)
TEST_F(BufferAllocatorTest, NonAscendingOrder) {
    auto strat256 = make_malloc_strategy(256);
    auto strat64 = make_malloc_strategy(64);
    auto strat128 = make_malloc_strategy(128);
    
    ftl::allocator::BufferAllocator<3> allocator({
        strat256.get(), strat64.get(), strat128.get()
    });
    
    // After sorting, strategies should be: [64, 128, 256]
    // Should find 64-byte slot (now at index 0 after sorting)
    auto buf1 = allocator.allocate(32);
    ASSERT_TRUE(buf1);
    EXPECT_EQ(buf1.size, 64);
    EXPECT_EQ(buf1.slot_index, 0);  // First slot after sorting
    
    // Should find 128-byte slot (now at index 1 after sorting)
    auto buf2 = allocator.allocate(100);
    ASSERT_TRUE(buf2);
    EXPECT_EQ(buf2.size, 128);
    EXPECT_EQ(buf2.slot_index, 1);  // Second slot after sorting
    
    // Should find 256-byte slot (now at index 2 after sorting)
    auto buf3 = allocator.allocate(200);
    ASSERT_TRUE(buf3);
    EXPECT_EQ(buf3.size, 256);
    EXPECT_EQ(buf3.slot_index, 2);  // Third slot after sorting
    
    allocator.deallocate(buf1);
    allocator.deallocate(buf2);
    allocator.deallocate(buf3);
}