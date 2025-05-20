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

// // Ensure that the allocator head only advances by the minimum required (the rounded-up payload size)
// TEST_F(BufferBumpPoolTest, AcquireMinimalHeadAdvance) {
//     std::vector<size_t> sizes = {1, 64, 100, 128, 200, 2048};
//     for (auto sz : sizes) {
//         auto headBefore = alloc_->head();
//         ftl::Buffer* buf = pool_->acquire(sz);
//         auto headAfter = alloc_->head();

//         // expected minimal advance is size rounded up to 64-byte increments
//         size_t expectedAdvance = ((sz + 63) / 64) * 64;
//         size_t actualAdvance = static_cast<size_t>(headAfter - headBefore);

//         EXPECT_EQ(actualAdvance, expectedAdvance)
//             << "Head advanced " << actualAdvance << " bytes but expected " << expectedAdvance
//             << " for request size=" << sz;

//         pool_->release(buf);
//     }
// }

TEST_F(BufferBumpPoolTest, AllocatorHeadAdvancesOnNewAllocation) {
    // head should move on first allocation
    auto headBefore = alloc_->head();
    ftl::Buffer* buf = pool_->acquire(123);
    auto headAfter = alloc_->head();
    EXPECT_GT(headAfter, headBefore)
        << "Allocator head did not advance on new allocation";
    pool_->release(buf);
}

TEST_F(BufferBumpPoolTest, ReleaseAllowsReuse) {
    // first acquire should advance head
    auto headBefore = alloc_->head();
    ftl::Buffer* buf1 = pool_->acquire(100);
    auto headAfterFirst = alloc_->head();
    EXPECT_GT(headAfterFirst, headBefore)
        << "Allocator head should advance on first acquire";
    pool_->release(buf1);

    // reuse should not advance head
    auto headBeforeSecond = alloc_->head();
    ftl::Buffer* buf2 = pool_->acquire(100);
    auto headAfterSecond = alloc_->head();
    EXPECT_EQ(headAfterSecond, headBeforeSecond)
        << "Allocator head should not advance when reusing buffer";
    EXPECT_EQ(buf1->front(), buf2->front())
        << "Released buffer should be reused for same size";
}

TEST_F(BufferBumpPoolTest, DifferentSlotsAreSeparated) {
    // 64→slot0, 65→slot1
    auto head0 = alloc_->head();

    ftl::Buffer* buf64  = pool_->acquire(64);
    auto head1 = alloc_->head();
    EXPECT_GT(head1, head0) << "Head should advance on slot0 allocation";

    ftl::Buffer* buf65  = pool_->acquire(65);
    auto head2 = alloc_->head();
    EXPECT_GT(head2, head1) << "Head should advance on slot1 allocation";

    EXPECT_NE(buf64->front(), buf65->front())
        << "Different slots must not collide";

    pool_->release(buf64);
    pool_->release(buf65);

    // reusing slot0
    auto headBeforeReuse64 = alloc_->head();
    ftl::Buffer* buf64b = pool_->acquire(64);
    auto headAfterReuse64 = alloc_->head();
    EXPECT_EQ(headAfterReuse64, headBeforeReuse64)
        << "Allocator head should not advance when reusing slot0";
    EXPECT_EQ(buf64->front(), buf64b->front());

    // reusing slot1
    auto headBeforeReuse65 = alloc_->head();
    ftl::Buffer* buf65b = pool_->acquire(65);
    auto headAfterReuse65 = alloc_->head();
    EXPECT_EQ(headAfterReuse65, headBeforeReuse65)
        << "Allocator head should not advance when reusing slot1";
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
        ptrs.push_back(pool_->acquire(sz));
        szs.push_back(sz);
        EXPECT_EQ(reinterpret_cast<uintptr_t>(ptrs.back()->front()) % 64, 0u);
    }
    // release in reverse
    for (size_t idx = ptrs.size(); idx-- > 0; ) {
        pool_->release(ptrs[idx]);
    }
    // re-acquire in original order: no head advance, buffers match
    for (size_t i = 0; i < ptrs.size(); ++i) {
        auto headBefore = alloc_->head();
        auto p = pool_->acquire(szs[i]);
        auto headAfter = alloc_->head();
        EXPECT_EQ(headAfter, headBefore) << "No head advance on reuse at index " << i;
        EXPECT_EQ(p->front(), ptrs[i]->front()) << "Mismatch at index " << i;
    }
}
