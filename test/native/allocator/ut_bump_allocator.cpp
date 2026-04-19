// ut_bump_allocator.cpp
//
// Unit tests for the BumpAllocator class.
// This file tests various functionalities of BumpAllocator:
//   - Allocation returns a valid pointer.
//   - Allocated memory is properly aligned.
//   - Allocation fails (returns nullptr) when out of memory.
//   - Allocation succeeds when exactly filling the block.
//   - The templated allocate() method constructs an object correctly.
//
// According to the Google C++ Style Guide, file names, function names, and comments
// follow consistent formatting for clarity and maintainability.

#include "ftl/allocator/bump_allocator.hpp"
#include "gtest/gtest.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

// A simple test class for use with the templated allocate() method.
class TestObject {
 public:
  explicit TestObject(int value) : value_(value) {}
  int value_;
};

namespace {
// Constants for the test memory block.
constexpr size_t kBlockSize = 1024;
// A static memory block used by the allocator in tests.
uint8_t gMemoryBlock[kBlockSize];
}  // namespace

// Test fixture for BumpAllocator tests.
class BumpAllocatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Reset the memory block to zeros.
    std::memset(gMemoryBlock, 0, kBlockSize);
    // Construct the bump allocator using the test memory block.
    allocator_ = new ftl::BumpAllocator(gMemoryBlock, kBlockSize);
  }

  void TearDown() override {
    delete allocator_;
  }

  ftl::BumpAllocator* allocator_;
};

// Tests that allocate() returns a valid (non-null) pointer when memory is available.
TEST_F(BumpAllocatorTest, AllocateReturnsValidPointer) {
  void* ptr = allocator_->allocate(100);
  EXPECT_NE(ptr, nullptr);
}

// Tests that allocate() returns a pointer aligned to the specified boundary.
TEST_F(BumpAllocatorTest, AllocateProperlyAlignsMemory) {
  // Allocate 50 bytes with a 16-byte alignment requirement.
  void* ptr = allocator_->allocate(50, 16);
  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  EXPECT_EQ(addr % 16, 0u);
}

// Tests that allocate() returns nullptr when there is not enough memory.
TEST_F(BumpAllocatorTest, AllocateReturnsNullWhenOutOfMemory) {
  // Try to allocate more bytes than available.
  void* ptr = allocator_->allocate(kBlockSize + 1);
  EXPECT_EQ(ptr, nullptr);
}

// Tests that allocation of the exact block size succeeds.
TEST_F(BumpAllocatorTest, AllocateExactBlockSizeSucceeds) {
  void* ptr = allocator_->allocate(kBlockSize);
  EXPECT_NE(ptr, nullptr);
}

// Tests that allocating one more byte after filling the block fails.
TEST_F(BumpAllocatorTest, AllocateOneByteTooManyAfterExact) {
  void* ptr1 = allocator_->allocate(kBlockSize);
  ASSERT_NE(ptr1, nullptr);
  // Now the block is exhausted; a further tiny allocation must fail.
  void* ptr2 = allocator_->allocate(1);
  EXPECT_EQ(ptr2, nullptr);
}

// Tests the templated allocate() method which constructs an object in allocated memory.
TEST_F(BumpAllocatorTest, TemplateAllocateConstructsObject) {
  TestObject* obj = allocator_->allocate<TestObject>(123);
  ASSERT_NE(obj, nullptr);
  EXPECT_EQ(obj->value_, 123);
}

// Tests that cache alignment prevents allocations from crossing cache line boundaries
TEST_F(BumpAllocatorTest, CacheAlignmentPreventsLineCrossing) {
  // Create allocator with cache alignment enabled
  alignas(64) uint8_t cache_arena[1024];
  ftl::BumpAllocator cache_allocator(cache_arena, sizeof(cache_arena), true);
  
  // Test small allocation that would cross cache line
  // First, allocate something that puts us near a cache line boundary
  void* ptr1 = cache_allocator.allocate(60);  // Takes most of first cache line
  ASSERT_NE(ptr1, nullptr);
  
  // This allocation would cross into next cache line without cache alignment
  void* ptr2 = cache_allocator.allocate(10);
  ASSERT_NE(ptr2, nullptr);
  
  // Verify ptr2 doesn't cross cache line boundary
  auto addr2 = reinterpret_cast<uintptr_t>(ptr2);
  auto cache_line_start = addr2 & ~(ftl::BumpAllocator::kCacheLineSize - 1);
  auto cache_line_end = cache_line_start + ftl::BumpAllocator::kCacheLineSize;
  EXPECT_LE(addr2 + 10, cache_line_end) << "Allocation crosses cache line boundary";
  
  // With cache alignment, ptr2 should have been bumped to the next cache line
  EXPECT_GE(addr2, reinterpret_cast<uintptr_t>(ptr1) + 60);
}

// Tests that cache alignment is disabled by default
TEST_F(BumpAllocatorTest, CacheAlignmentDisabledByDefault) {
  // Default allocator should not have cache alignment
  uint8_t default_arena[256];
  ftl::BumpAllocator default_allocator(default_arena, sizeof(default_arena));
  
  // Allocate near cache line boundary
  void* ptr1 = default_allocator.allocate(60);
  ASSERT_NE(ptr1, nullptr);
  
  void* ptr2 = default_allocator.allocate(10);
  ASSERT_NE(ptr2, nullptr);
  
  // Without cache alignment, ptr2 should follow ptr1 with default alignment
  auto addr1 = reinterpret_cast<uintptr_t>(ptr1);
  auto addr2 = reinterpret_cast<uintptr_t>(ptr2);
  auto expected_addr2 = (addr1 + 60 + alignof(std::max_align_t) - 1) & ~(alignof(std::max_align_t) - 1);
  EXPECT_EQ(addr2, expected_addr2);
}

// Tests cache alignment with larger alignment requirements
TEST_F(BumpAllocatorTest, CacheAlignmentWithLargerAlignment) {
  alignas(128) uint8_t cache_arena[1024];
  ftl::BumpAllocator cache_allocator(cache_arena, sizeof(cache_arena), true);
  
  // Request 128-byte alignment (larger than cache line)
  void* ptr = cache_allocator.allocate(64, 128);
  ASSERT_NE(ptr, nullptr);
  
  auto addr = reinterpret_cast<uintptr_t>(ptr);
  // Should be aligned to 128 bytes
  EXPECT_EQ(addr % 128, 0u);
  
  // Should also not cross cache line (though 128-byte alignment guarantees this)
  auto cache_line_start = addr & ~(ftl::BumpAllocator::kCacheLineSize - 1);
  auto cache_line_end = cache_line_start + ftl::BumpAllocator::kCacheLineSize;
  EXPECT_LE(addr + 64, cache_line_end + ftl::BumpAllocator::kCacheLineSize);
}

// Tests that all allocations are cache-aligned when cache alignment is enabled
TEST_F(BumpAllocatorTest, CacheAlignmentAppliesToAllAllocations) {
  alignas(64) uint8_t cache_arena[512];
  ftl::BumpAllocator cache_allocator(cache_arena, sizeof(cache_arena), true);
  
  // Allocate something small first
  void* ptr1 = cache_allocator.allocate(10);
  ASSERT_NE(ptr1, nullptr);
  
  // Allocate something larger than cache line
  constexpr size_t large_size = ftl::BumpAllocator::kCacheLineSize + 20;
  void* ptr2 = cache_allocator.allocate(large_size);
  ASSERT_NE(ptr2, nullptr);
  
  // With cache alignment enabled, all allocations should start at cache line boundaries
  auto addr1 = reinterpret_cast<uintptr_t>(ptr1);
  auto addr2 = reinterpret_cast<uintptr_t>(ptr2);
  
  // Both allocations should be cache-aligned
  EXPECT_EQ(addr1 % ftl::BumpAllocator::kCacheLineSize, 0u) << "First allocation should be cache-aligned";
  EXPECT_EQ(addr2 % ftl::BumpAllocator::kCacheLineSize, 0u) << "Large allocation should also be cache-aligned";
  
  // ptr2 should be at least one cache line after ptr1
  EXPECT_GE(addr2 - addr1, ftl::BumpAllocator::kCacheLineSize) << "Large allocation should start at next cache line";
}

// Stress allocate() from many threads and verify that no two returned slots overlap.
TEST(BumpAllocatorConcurrent, NoOverlap) {
  alignas(16) uint8_t arena[16 * 1024];
  ftl::BumpAllocator alloc(arena, sizeof(arena));

  constexpr size_t kThreads = 8;
  constexpr size_t kPerThread = 64;
  constexpr size_t kSize = 16;

  std::vector<std::vector<uint8_t*>> results(kThreads);
  std::atomic<bool> go{false};

  auto worker = [&](size_t id) {
    while (!go.load(std::memory_order_acquire)) {}
    for (size_t i = 0; i < kPerThread; ++i) {
      results[id].push_back(static_cast<uint8_t*>(alloc.allocate(kSize)));
    }
  };

  std::vector<std::thread> threads;
  for (size_t i = 0; i < kThreads; ++i) threads.emplace_back(worker, i);
  go.store(true, std::memory_order_release);
  for (auto& t : threads) t.join();

  std::vector<uint8_t*> flat;
  for (const auto& v : results) flat.insert(flat.end(), v.begin(), v.end());
  std::sort(flat.begin(), flat.end());

  for (uint8_t* p : flat) ASSERT_NE(p, nullptr);
  for (size_t i = 1; i < flat.size(); ++i) {
    EXPECT_GE(flat[i], flat[i - 1] + kSize) << "overlap between allocations";
  }
}
