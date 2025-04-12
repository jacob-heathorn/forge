// ut_bump_allocator.cpp
//
// Unit tests for the BumpAllocator class.
// This file tests various functionalities of BumpAllocator:
//   - Allocation returns a valid pointer.
//   - Allocated memory is properly aligned.
//   - Allocation fails (returns nullptr) when out of memory.
//   - The templated allocate() method constructs an object correctly.
//   - The reset() method allows reusing the memory block.
//
// According to the Google C++ Style Guide, file names, function names, and comments
// follow consistent formatting for clarity and maintainability.

#include "ftl/bump_allocator.hpp"
#include "gtest/gtest.h"
#include <cstdint>
#include <cstring>

// A simple test class for use with the templated allocate() method.
class TestObject {
 public:
  explicit TestObject(int value) : value_(value) {}
  int value_;
};

namespace {
  // Constants for the test memory block.
  const size_t kBlockSize = 1024;
  // A static memory block used by the allocator in tests.
  uint8_t gMemoryBlock[kBlockSize];
}  // namespace

// Test fixture for BumpAllocator tests.
class BumpAllocatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Reset the memory block to zeros.
    memset(gMemoryBlock, 0, kBlockSize);
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
  EXPECT_EQ(addr % 16, 0);
}

// Tests that allocate() returns nullptr when there is not enough memory.
TEST_F(BumpAllocatorTest, AllocateReturnsNullWhenOutOfMemory) {
  // Try to allocate more bytes than available.
  void* ptr = allocator_->allocate(kBlockSize + 1);
  EXPECT_EQ(ptr, nullptr);
}

// Tests the templated allocate() method which constructs an object in allocated memory.
TEST_F(BumpAllocatorTest, TemplateAllocateConstructsObject) {
  TestObject* obj = allocator_->allocate<TestObject>(123);
  ASSERT_NE(obj, nullptr);
  EXPECT_EQ(obj->value_, 123);
}

// Tests that reset() correctly resets the allocator so that memory can be reallocated.
// After resetting, a new allocation should start at the same address as the first allocation.
TEST_F(BumpAllocatorTest, ResetAllowsReallocation) {
  void* ptr1 = allocator_->allocate(200);
  EXPECT_NE(ptr1, nullptr);
  allocator_->reset();
  void* ptr2 = allocator_->allocate(200);
  EXPECT_EQ(ptr1, ptr2);
}
