#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include "etl/memory.h"

#include "ftl/bump_allocator.hpp"
#include "ftl/bump_pool.hpp"

// Size of the bump-allocator backing store (1 MB)
static constexpr size_t POOL_MEMORY_SIZE = 1024 * 1024;


//------------------------------------------------------------------------------
// Helper type to count constructions and destructions
//------------------------------------------------------------------------------
struct CountingType {
  static std::atomic<int> ctor_count;
  static std::atomic<int> dtor_count;
  int val;

  CountingType(int v = 0) : val(v) {
    ++ctor_count;
  }

  ~CountingType() {
    ++dtor_count;
  }
};

std::atomic<int> CountingType::ctor_count{0};
std::atomic<int> CountingType::dtor_count{0};

//------------------------------------------------------------------------------
// Test that acquire returns a non-null pointer and release works
//------------------------------------------------------------------------------
TEST(BumpPoolTest, AcquireReturnsNonNull) {
  // Allocate backing memory with new
  auto* buffer = new uint8_t[POOL_MEMORY_SIZE];
  ftl::BumpAllocator alloc(buffer, POOL_MEMORY_SIZE);
  BumpPool<int> pool(alloc);

  int* p = pool.acquire();
  EXPECT_NE(p, nullptr);
  pool.release(p);

  delete[] buffer;
}

//------------------------------------------------------------------------------
// Test that releasing a null pointer is a no-op
//------------------------------------------------------------------------------
TEST(BumpPoolTest, ReleaseNullIsNoOp) {
  auto* buffer = new uint8_t[POOL_MEMORY_SIZE];
  ftl::BumpAllocator alloc(buffer, POOL_MEMORY_SIZE);
  BumpPool<int> pool(alloc);

  EXPECT_NO_THROW(pool.release(nullptr));

  delete[] buffer;
}

//------------------------------------------------------------------------------
// Test that constructors and destructors of T are called correctly
//------------------------------------------------------------------------------
TEST(BumpPoolCountingType, ConstructorDestructorCount) {
  auto* buffer = new uint8_t[POOL_MEMORY_SIZE];
  ftl::BumpAllocator alloc(buffer, POOL_MEMORY_SIZE);
  BumpPool<CountingType> pool(alloc, /*initialSize=*/0);

  // Reset counters
  CountingType::ctor_count = 0;
  CountingType::dtor_count = 0;

  // Acquire with argument and check construction
  CountingType* obj = pool.acquire(42);
  EXPECT_EQ(CountingType::ctor_count.load(), 1);
  EXPECT_EQ(obj->val, 42);

  // Release and check destruction
  pool.release(obj);
  EXPECT_EQ(CountingType::dtor_count.load(), 1);

  delete[] buffer;
}

//------------------------------------------------------------------------------
// Test that released memory is reused
//------------------------------------------------------------------------------
TEST(BumpPoolTest, ReuseMemoryAfterRelease) {
  auto* buffer = new uint8_t[POOL_MEMORY_SIZE];
  ftl::BumpAllocator alloc(buffer, POOL_MEMORY_SIZE);
  BumpPool<int> pool(alloc, /*initialSize=*/1);

  int* first = pool.acquire();
  pool.release(first);
  int* second = pool.acquire();

  EXPECT_EQ(second, first);
  pool.release(second);

  delete[] buffer;
}

//------------------------------------------------------------------------------
// Initial counters should reflect the preallocation
//------------------------------------------------------------------------------
TEST(BumpPoolSizeTest, InitialCounts) {
  auto* buffer = new uint8_t[POOL_MEMORY_SIZE];
  ftl::BumpAllocator alloc(buffer, POOL_MEMORY_SIZE);
  BumpPool<int> pool(alloc, /*initialSize=*/5);

  EXPECT_EQ(pool.TotalSize(), 5);
  EXPECT_EQ(pool.FreeSize(), 5);
  EXPECT_EQ(pool.UsedSize(), 0);

  delete[] buffer;
}

//------------------------------------------------------------------------------
// Acquiring and releasing adjusts free/used counts correctly
//------------------------------------------------------------------------------
TEST(BumpPoolSizeTest, SingleAcquireRelease) {
  auto* buffer = new uint8_t[POOL_MEMORY_SIZE];
  ftl::BumpAllocator alloc(buffer, POOL_MEMORY_SIZE);
  BumpPool<int> pool(alloc, /*initialSize=*/3);

  int* a = pool.acquire();
  EXPECT_EQ(pool.TotalSize(), 3);
  EXPECT_EQ(pool.FreeSize(), 2);
  EXPECT_EQ(pool.UsedSize(), 1);

  pool.release(a);
  EXPECT_EQ(pool.FreeSize(), 3);
  EXPECT_EQ(pool.UsedSize(), 0);

  delete[] buffer;
}

//------------------------------------------------------------------------------
// Exhausting the free-list causes bump-allocation, then counters update
//------------------------------------------------------------------------------
TEST(BumpPoolSizeTest, ExhaustAndExpand) {
  auto* buffer = new uint8_t[POOL_MEMORY_SIZE];
  ftl::BumpAllocator alloc(buffer, POOL_MEMORY_SIZE);
  BumpPool<int> pool(alloc, /*initialSize=*/2);

  // Pop both prebuilt nodes
  int* x = pool.acquire();
  int* y = pool.acquire();
  EXPECT_EQ(pool.FreeSize(), 0);
  EXPECT_EQ(pool.UsedSize(), 2);
  EXPECT_EQ(pool.TotalSize(), 2);

  // Third acquire must bump-allocate a new node
  int* z = pool.acquire();
  EXPECT_EQ(pool.TotalSize(), 3);
  EXPECT_EQ(pool.FreeSize(), 0);
  EXPECT_EQ(pool.UsedSize(), 3);

  // Release one back
  pool.release(x);
  EXPECT_EQ(pool.FreeSize(), 1);
  EXPECT_EQ(pool.UsedSize(), 2);

  // Clean up
  pool.release(y);
  pool.release(z);
  EXPECT_EQ(pool.FreeSize(), 3);
  EXPECT_EQ(pool.UsedSize(), 0);

  delete[] buffer;
}

//------------------------------------------------------------------------------
// Stress test thread-safety: concurrent acquire/release using std::thread
//------------------------------------------------------------------------------
TEST(BumpPoolThreadSafety, AcquireReleaseConcurrently) {
  auto* buffer = new uint8_t[POOL_MEMORY_SIZE];
  ftl::BumpAllocator alloc(buffer, POOL_MEMORY_SIZE);
  BumpPool<CountingType> pool(alloc, /*initialSize=*/2);
  EXPECT_EQ(pool.TotalSize(), 2);
  EXPECT_EQ(pool.FreeSize(), 2);
  EXPECT_EQ(pool.UsedSize(), 0);

  // Reset counters
  CountingType::ctor_count = 0;
  CountingType::dtor_count = 0;

  const int iterations = 10;

  std::thread t1([&]() {
    for (int i = 0; i < iterations; ++i) {
      auto* obj = pool.acquire(i);
      pool.release(obj);
    }
  });

  std::thread t2([&]() {
    for (int i = 0; i < iterations; ++i) {
      auto* obj = pool.acquire(i);
      pool.release(obj);
    }
  });

  t1.join();
  t2.join();

  EXPECT_GE(pool.TotalSize(), 2);
  EXPECT_GE(pool.FreeSize(), 2);
  EXPECT_EQ(pool.UsedSize(), 0);

  EXPECT_EQ(CountingType::ctor_count.load(), 2 * iterations);
  EXPECT_EQ(CountingType::dtor_count.load(), 2 * iterations);

  delete[] buffer;
}

//------------------------------------------------------------------------------
// Test automatic release via unique_ptr custom deleter
//------------------------------------------------------------------------------
TEST(BumpPoolTest, UniquePtrAutomaticRelease) {
  auto* buffer = new uint8_t[POOL_MEMORY_SIZE];
  ftl::BumpAllocator alloc(buffer, POOL_MEMORY_SIZE);
  BumpPool<int> pool(alloc, /*initialSize=*/1);

  // Initially exactly one free slot, none in use
  EXPECT_EQ(pool.FreeSize(), 1);
  EXPECT_EQ(pool.UsedSize(), 0);

  {
    // Create a unique_ptr that will call pool.release(...) when destroyed
    auto deleter = [&](int* p){ pool.release(p); };
    etl::unique_ptr<int, decltype(deleter)> ptr(pool.acquire(123), deleter);

    // The pointer holds our value, and the pool is now empty/1 in use
    EXPECT_EQ(*ptr, 123);
    EXPECT_EQ(pool.FreeSize(), 0);
    EXPECT_EQ(pool.UsedSize(), 1);

    // Exiting this scope will destroy ptr and call pool.release(ptr.get())
  }

  // After scope exit, the slot is returned automatically
  EXPECT_EQ(pool.FreeSize(), 1);
  EXPECT_EQ(pool.UsedSize(), 0);

  delete[] buffer;
}
