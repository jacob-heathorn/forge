#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <memory>

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
// Test fixture for BumpPool tests
//------------------------------------------------------------------------------
class BumpPoolTest : public ::testing::Test {
protected:
  uint8_t* buffer_;
  ftl::BumpAllocator* alloc_;

  void SetUp() override {
    buffer_ = new uint8_t[POOL_MEMORY_SIZE];
    alloc_  = new ftl::BumpAllocator(buffer_, POOL_MEMORY_SIZE);
  }

  void TearDown() override {
    delete alloc_;
    delete[] buffer_;
  }
};

//------------------------------------------------------------------------------
// Acquire returns non-null and release works
//------------------------------------------------------------------------------
TEST_F(BumpPoolTest, AcquireReturnsNonNull) {
  BumpPool<int> pool(*alloc_);

  int* p = pool.acquire();
  EXPECT_NE(p, nullptr);
  pool.release(p);
}

//------------------------------------------------------------------------------
// Release null is no-op
//------------------------------------------------------------------------------
TEST_F(BumpPoolTest, ReleaseNullIsNoOp) {
  BumpPool<int> pool(*alloc_);

  EXPECT_NO_THROW(pool.release(nullptr));
}

//------------------------------------------------------------------------------
// Constructors and destructors are called correctly
//------------------------------------------------------------------------------
TEST_F(BumpPoolTest, ConstructorDestructorCount) {
  BumpPool<CountingType> pool(*alloc_, /*initialSize=*/0);

  CountingType::ctor_count = 0;
  CountingType::dtor_count = 0;

  CountingType* obj = pool.acquire(42);
  EXPECT_EQ(CountingType::ctor_count.load(), 1);
  EXPECT_EQ(obj->val, 42);

  pool.release(obj);
  EXPECT_EQ(CountingType::dtor_count.load(), 1);
}

//------------------------------------------------------------------------------
// Released memory is reused
//------------------------------------------------------------------------------
TEST_F(BumpPoolTest, ReuseMemoryAfterRelease) {
  BumpPool<int> pool(*alloc_, /*initialSize=*/1);

  int* first  = pool.acquire();
  pool.release(first);
  int* second = pool.acquire();

  EXPECT_EQ(second, first);
  pool.release(second);
}

//------------------------------------------------------------------------------
// Initial counts reflect preallocation
//------------------------------------------------------------------------------
TEST_F(BumpPoolTest, InitialCounts) {
  BumpPool<int> pool(*alloc_, /*initialSize=*/5);

  EXPECT_EQ(pool.TotalSize(), 5);
  EXPECT_EQ(pool.FreeSize(), 5);
  EXPECT_EQ(pool.UsedSize(), 0);
}

//------------------------------------------------------------------------------
// Single acquire/release adjusts counts
//------------------------------------------------------------------------------
TEST_F(BumpPoolTest, SingleAcquireRelease) {
  BumpPool<int> pool(*alloc_, /*initialSize=*/3);

  int* a = pool.acquire();
  EXPECT_EQ(pool.TotalSize(), 3);
  EXPECT_EQ(pool.FreeSize(), 2);
  EXPECT_EQ(pool.UsedSize(), 1);

  pool.release(a);
  EXPECT_EQ(pool.FreeSize(), 3);
  EXPECT_EQ(pool.UsedSize(), 0);
}

//------------------------------------------------------------------------------
// Exhausting free-list causes bump-allocation
//------------------------------------------------------------------------------
TEST_F(BumpPoolTest, ExhaustAndExpand) {
  BumpPool<int> pool(*alloc_, /*initialSize=*/2);

  int* x = pool.acquire();
  int* y = pool.acquire();
  EXPECT_EQ(pool.FreeSize(), 0);
  EXPECT_EQ(pool.UsedSize(), 2);
  EXPECT_EQ(pool.TotalSize(), 2);

  int* z = pool.acquire();
  EXPECT_EQ(pool.TotalSize(), 3);
  EXPECT_EQ(pool.FreeSize(), 0);
  EXPECT_EQ(pool.UsedSize(), 3);

  pool.release(x);
  EXPECT_EQ(pool.FreeSize(), 1);
  EXPECT_EQ(pool.UsedSize(), 2);

  pool.release(y);
  pool.release(z);
  EXPECT_EQ(pool.FreeSize(), 3);
  EXPECT_EQ(pool.UsedSize(), 0);
}

//------------------------------------------------------------------------------
// Thread-safety: concurrent acquire/release
//------------------------------------------------------------------------------
TEST_F(BumpPoolTest, AcquireReleaseConcurrently) {
  BumpPool<CountingType> pool(*alloc_, /*initialSize=*/2);

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
}

//------------------------------------------------------------------------------
// Unique_ptr custom deleter automatic release
//------------------------------------------------------------------------------
TEST_F(BumpPoolTest, UniquePtrAutomaticRelease) {
  BumpPool<int> pool(*alloc_, /*initialSize=*/1);

  EXPECT_EQ(pool.FreeSize(), 1);
  EXPECT_EQ(pool.UsedSize(), 0);

  {
    auto deleter = [&](int* p) { pool.release(p); };
    std::unique_ptr<int, decltype(deleter)> ptr(pool.acquire(123), deleter);

    EXPECT_EQ(*ptr, 123);
    EXPECT_EQ(pool.FreeSize(), 0);
    EXPECT_EQ(pool.UsedSize(), 1);
  }

  EXPECT_EQ(pool.FreeSize(), 1);
  EXPECT_EQ(pool.UsedSize(), 0);
}
