#include <gtest/gtest.h>

#include <atomic>
#include <thread>

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
// Stress test thread-safety: concurrent acquire/release using std::thread
//------------------------------------------------------------------------------
TEST(BumpPoolThreadSafety, AcquireReleaseConcurrently) {
  auto* buffer = new uint8_t[POOL_MEMORY_SIZE];
  ftl::BumpAllocator alloc(buffer, POOL_MEMORY_SIZE);
  BumpPool<CountingType> pool(alloc, /*initialSize=*/2);

  // Reset counters
  CountingType::ctor_count = 0;
  CountingType::dtor_count = 0;

  const int iterations = 10000;

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

  EXPECT_EQ(CountingType::ctor_count.load(), 2 * iterations);
  EXPECT_EQ(CountingType::dtor_count.load(), 2 * iterations);

  delete[] buffer;
}
