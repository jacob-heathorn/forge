#include <gtest/gtest.h>
#include <vector>
#include <cstdint>

#include "ftl/buffer_bump_pool.hpp"
#include "ftl/bump_allocator.hpp"

class BufferBumpPoolTest : public ::testing::Test {
protected:
    static constexpr size_t POOL_MEMORY_SIZE = 16 * 1024;  // 16 KiB for tests
    uint8_t*              buffer_;
    ftl::BumpAllocator*   alloc_;
    ftl::BufferBumpPool*  pool_;

    void SetUp() override {
        buffer_ = new uint8_t[POOL_MEMORY_SIZE];
        alloc_  = new ftl::BumpAllocator(buffer_, POOL_MEMORY_SIZE);
        pool_   = new ftl::BufferBumpPool(*alloc_);
    }

    void TearDown() override {
        delete pool_;
        delete alloc_;
        delete[] buffer_;
    }
};

TEST_F(BufferBumpPoolTest, AcquireReturnsAlignedBuffer) {
    // various sizes, including boundary and clamp
    std::vector<size_t> sizes = {1, 64, 100, 128, 200, 2048};
    for (auto sz : sizes) {
        ftl::Buffer* buf = pool_->acquire(sz);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(buf->front()) % 64, 0u)
            << "Buffer not 64-byte aligned for size=" << sz;
        pool_->release(buf);
    }
}

TEST_F(BufferBumpPoolTest, ReleaseAllowsReuse) {
    ftl::Buffer* buf1 = pool_->acquire(100);
    pool_->release(buf1);
    ftl::Buffer* buf2 = pool_->acquire(100);
    EXPECT_EQ(buf1->front(), buf2->front())
        << "Released buffer should be reused for same size";
}

TEST_F(BufferBumpPoolTest, DifferentSlotsAreSeparated) {
    // 64→slot0, 65→slot1
    ftl::Buffer* buf64  = pool_->acquire(64);
    ftl::Buffer* buf65  = pool_->acquire(65);
    EXPECT_NE(buf64->front(), buf65->front())
        << "Different slots must not collide";
    pool_->release(buf64);
    pool_->release(buf65);

    // Re-acquire: each should return its own
    ftl::Buffer* buf64b = pool_->acquire(64);
    ftl::Buffer* buf65b = pool_->acquire(65);
    EXPECT_EQ(buf64->front(), buf64b->front());
    EXPECT_EQ(buf65->front(), buf65b->front());
}

TEST_F(BufferBumpPoolTest, AcquireOversizeTriggersAssert) {
    // Asking for more than MAX_SIZE should hit our assert() and abort.
    EXPECT_DEATH(
        { pool_->acquire(5000); },
        "Requested size exceeds maximum buffer size"
    );
}

TEST_F(BufferBumpPoolTest, MultipleAllocationsDontCorrupt) {
    // allocate many buffers of varying sizes, then release out‐of‐order
    std::vector<ftl::Buffer*> ptrs;
    std::vector<size_t>        szs;
    for (size_t i = 1; i <= 10; ++i) {
        size_t sz = i * 123;            // some arbitrary sizes
        auto   p  = pool_->acquire(sz);
        ptrs.push_back(p);
        szs.push_back(sz);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(p->front()) % 64, 0u);
    }
    // release in reverse
    for (size_t idx = ptrs.size(); idx-- > 0; ) {
        pool_->release(ptrs[idx]);
    }
    // re-acquire in original order, should get same pointers
    for (size_t i = 0; i < ptrs.size(); ++i) {
        auto p = pool_->acquire(szs[i]);
        EXPECT_EQ(p->front(), ptrs[i]->front()) << "Mismatch at index " << i;
    }
}
