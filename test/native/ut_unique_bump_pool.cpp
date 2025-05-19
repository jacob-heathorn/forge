#include <gtest/gtest.h>

#include "ftl/unique_bump_pool.hpp"
#include "ftl/bump_pool.hpp"

// Size of the bump-allocator backing store (1 MB)
static constexpr size_t POOL_MEMORY_SIZE = 1024 * 1024;

// A simple type for testing pool allocation and construction
struct TestInt {
  explicit TestInt(int v) : value(v) {}
  int value;
};

// A polymorphic base/derived pair for testing type-erased deletion
struct Base {
  virtual ~Base() = default;
  virtual int id() const = 0;
};
struct Derived : Base {
  explicit Derived(int v) : value(v) {}
  int id() const override { return value; }
  int value;
};

// Test fixture for UniqueBumpPool tests
class UniqueBumpPoolTest : public ::testing::Test {
protected:
  void SetUp() override {
    buffer_ = new uint8_t[POOL_MEMORY_SIZE];
    allocator_ = new ftl::BumpAllocator(buffer_, POOL_MEMORY_SIZE);
  }

  void TearDown() override {
    delete allocator_;
    delete[] buffer_;
  }

  uint8_t* buffer_ = nullptr;
  ftl::BumpAllocator* allocator_ = nullptr;
};

// Test basic acquire and element access for trivial type
TEST_F(UniqueBumpPoolTest, AcquireReturnsValidUniquePtr) {
  UniqueBumpPool<TestInt> pool{*allocator_, /*initialSize=*/1};
  auto ptr = pool.acquire(123);

  ASSERT_NE(ptr.get(), nullptr);
  EXPECT_EQ(ptr->value, 123);
}

// Test that releasing and re-acquiring reuses the same memory address
TEST_F(UniqueBumpPoolTest, ReleaseAndAcquireReuseMemory) {
  UniqueBumpPool<TestInt> pool{*allocator_, /*initialSize=*/1};

  uintptr_t first_addr;
  {
    auto ptr = pool.acquire(1);
    first_addr = reinterpret_cast<uintptr_t>(ptr.get());
    // ptr goes out of scope here, invoking pool.release
  }

  auto ptr2 = pool.acquire(2);
  uintptr_t second_addr = reinterpret_cast<uintptr_t>(ptr2.get());
  EXPECT_EQ(second_addr, first_addr);
  EXPECT_EQ(ptr2->value, 2);
}

// Test polymorphic behavior with Base/Derived
TEST_F(UniqueBumpPoolTest, PolymorphicDeleterWorksWithDerived) {
  UniqueBumpPool<Derived, Base> pool{*allocator_, /*initialSize=*/1};
  auto ptr = pool.acquire(42);

  ASSERT_NE(ptr.get(), nullptr);
  EXPECT_EQ(ptr->id(), 42);
}

// Test deleter type matches DelegatingDeleter<TestInt>
TEST_F(UniqueBumpPoolTest, UniquePtrUsesDelegatingDeleter) {
  UniqueBumpPool<TestInt> pool{*allocator_, /*initialSize=*/1};
  auto ptr = pool.acquire(7);

  using DeleterT = typename decltype(ptr)::deleter_type;
  EXPECT_TRUE((std::is_same<DeleterT, DelegatingDeleter<TestInt>>::value));
}

// Test TotalSize, FreeSize, UsedSize after construction
TEST_F(UniqueBumpPoolTest, InitialTotalFreeUsedSize) {
  UniqueBumpPool<TestInt> pool{*allocator_, /*initialSize=*/3};
  EXPECT_EQ(pool.TotalSize(), 3u);
  EXPECT_EQ(pool.FreeSize(), 3u);
  EXPECT_EQ(pool.UsedSize(), 0u);
}

// Test counts update on acquire and release
TEST_F(UniqueBumpPoolTest, TotalFreeUsedSizeOnAcquireAndRelease) {
  UniqueBumpPool<TestInt> pool{*allocator_, /*initialSize=*/2};
  EXPECT_EQ(pool.TotalSize(), 2u);
  EXPECT_EQ(pool.FreeSize(), 2u);
  EXPECT_EQ(pool.UsedSize(), 0u);

  {
    auto ptr = pool.acquire(5);
    EXPECT_EQ(pool.FreeSize(), 1u);
    EXPECT_EQ(pool.UsedSize(), 1u);
  }

  // After ptr is destroyed, pool.release() should have been called
  EXPECT_EQ(pool.FreeSize(), 2u);
  EXPECT_EQ(pool.UsedSize(), 0u);
}
