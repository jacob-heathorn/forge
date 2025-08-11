#include "ftl/allocator/strategy.hpp"
#include "ftl/allocator/bump_pool_strategy.hpp"
#include "ftl/bump_allocator.hpp"
#include "gtest/gtest.h"
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace {

struct TestObject {
    explicit TestObject(int v = 42) : value(v), active(true) {}
    ~TestObject() { active = false; }
    int value;
    bool active;
    char padding[64];  // Make it larger to test real allocations
};

struct AlignedObject {
    alignas(32) uint64_t data[4];
    explicit AlignedObject(uint64_t v = 0) {
        for (auto& d : data) d = v;
    }
};

constexpr size_t kArenaSize = 4096;
alignas(64) uint8_t g_arena[kArenaSize];

}  // namespace

class BumpPoolStrategyTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::memset(g_arena, 0, kArenaSize);
        bump_allocator_ = std::make_unique<ftl::BumpAllocator>(g_arena, kArenaSize);
    }

    void TearDown() override {
        bump_allocator_.reset();
    }
    
    std::unique_ptr<ftl::BumpAllocator> bump_allocator_;
};

// Test basic buffer allocation with BumpPoolBlockStrategy
TEST_F(BumpPoolStrategyTest, BufferAllocationBasic) {
    constexpr size_t block_size = 256;
    ftl::allocator::BumpPoolBlockStrategy strategy(*bump_allocator_, block_size);
    
    // Allocate a buffer
    void* ptr = strategy.allocate();
    ASSERT_NE(ptr, nullptr);
    
    // Write to the buffer to verify it's valid memory
    std::memset(ptr, 0xAB, block_size);
    
    // Deallocate
    strategy.deallocate(ptr);
    
    // Should be able to reuse the same memory
    void* ptr2 = strategy.allocate();
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(ptr, ptr2);  // Should get the same memory back from free list
    
    strategy.deallocate(ptr2);
}

// Test free list reuse
TEST_F(BumpPoolStrategyTest, FreeListReuse) {
    constexpr size_t block_size = 128;
    ftl::allocator::BumpPoolBlockStrategy strategy(*bump_allocator_, block_size);
    
    std::vector<void*> ptrs;
    constexpr size_t num_allocs = 5;
    
    // Allocate multiple blocks
    for (size_t i = 0; i < num_allocs; ++i) {
        void* ptr = strategy.allocate();
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
        std::memset(ptr, static_cast<uint8_t>(i), block_size);
    }
    
    // Deallocate all
    for (void* ptr : ptrs) {
        strategy.deallocate(ptr);
    }
    
    // Allocate again - should reuse from free list in LIFO order
    std::vector<void*> reused_ptrs;
    for (size_t i = 0; i < num_allocs; ++i) {
        void* ptr = strategy.allocate();
        ASSERT_NE(ptr, nullptr);
        reused_ptrs.push_back(ptr);
    }
    
    // Verify we got the same pointers back (in reverse order due to LIFO)
    for (size_t i = 0; i < num_allocs; ++i) {
        EXPECT_EQ(reused_ptrs[i], ptrs[num_allocs - 1 - i]);
    }
    
    // Cleanup
    for (void* ptr : reused_ptrs) {
        strategy.deallocate(ptr);
    }
}

// Test object allocation with BumpPoolObjStrategy
TEST_F(BumpPoolStrategyTest, ObjectAllocationBasic) {
    ftl::allocator::BumpPoolObjStrategy<TestObject> obj_strategy(*bump_allocator_);
    
    // Allocate raw memory for TestObject
    void* raw = obj_strategy.allocate();
    ASSERT_NE(raw, nullptr);
    
    // Construct object using placement new
    TestObject* obj = new (raw) TestObject(123);
    EXPECT_EQ(obj->value, 123);
    EXPECT_TRUE(obj->active);
    
    // Destroy object
    obj->~TestObject();
    EXPECT_FALSE(obj->active);
    
    // Deallocate memory
    obj_strategy.deallocate(raw);
    
    // Allocate again - should reuse
    void* raw2 = obj_strategy.allocate();
    ASSERT_NE(raw2, nullptr);
    EXPECT_EQ(raw, raw2);
    
    obj_strategy.deallocate(raw2);
}

// Test multiple object allocations with free list
TEST_F(BumpPoolStrategyTest, MultipleObjectAllocations) {
    ftl::allocator::BumpPoolObjStrategy<TestObject> obj_strategy(*bump_allocator_);
    
    std::vector<TestObject*> objects;
    constexpr size_t num_objects = 10;
    
    // Allocate multiple objects
    for (size_t i = 0; i < num_objects; ++i) {
        void* raw = obj_strategy.allocate();
        ASSERT_NE(raw, nullptr);
        TestObject* obj = new (raw) TestObject();
        obj->value = static_cast<int>(i * 10);
        objects.push_back(obj);
    }
    
    // Verify all objects are at different addresses
    for (size_t i = 0; i < num_objects; ++i) {
        for (size_t j = i + 1; j < num_objects; ++j) {
            EXPECT_NE(objects[i], objects[j]);
        }
    }
    
    // Verify values
    for (size_t i = 0; i < num_objects; ++i) {
        EXPECT_EQ(objects[i]->value, static_cast<int>(i * 10));
        EXPECT_TRUE(objects[i]->active);
    }
    
    // Deallocate half
    for (size_t i = 0; i < num_objects / 2; ++i) {
        objects[i]->~TestObject();
        obj_strategy.deallocate(objects[i]);
    }
    
    // Allocate new objects - should reuse deallocated memory
    std::vector<TestObject*> new_objects;
    for (size_t i = 0; i < num_objects / 2; ++i) {
        void* raw = obj_strategy.allocate();
        ASSERT_NE(raw, nullptr);
        TestObject* obj = new (raw) TestObject();
        new_objects.push_back(obj);
    }
    
    // Cleanup remaining
    for (size_t i = num_objects / 2; i < num_objects; ++i) {
        objects[i]->~TestObject();
        obj_strategy.deallocate(objects[i]);
    }
    
    for (TestObject* obj : new_objects) {
        obj->~TestObject();
        obj_strategy.deallocate(obj);
    }
}

// Test aligned object allocation
TEST_F(BumpPoolStrategyTest, AlignedObjectAllocation) {
    ftl::allocator::BumpPoolObjStrategy<AlignedObject> obj_strategy(*bump_allocator_);
    
    // Allocate aligned object
    void* raw = obj_strategy.allocate();
    ASSERT_NE(raw, nullptr);
    AlignedObject* obj = new (raw) AlignedObject();
    
    // Verify alignment
    auto addr = reinterpret_cast<std::uintptr_t>(obj);
    EXPECT_EQ(addr % alignof(AlignedObject), 0) << "Object not properly aligned";
    
    // Use the object
    for (auto& d : obj->data) {
        d = 0xDEADBEEF;
    }
    
    for (const auto& d : obj->data) {
        EXPECT_EQ(d, 0xDEADBEEF);
    }
    
    obj->~AlignedObject();
    obj_strategy.deallocate(raw);
}

// Test null pointer handling
TEST_F(BumpPoolStrategyTest, NullPointerHandling) {
    ftl::allocator::BumpPoolBlockStrategy strategy(*bump_allocator_, 64);
    
    // Deallocating nullptr should be safe
    strategy.deallocate(nullptr);
    
    ftl::allocator::BumpPoolObjStrategy<TestObject> obj_strategy(*bump_allocator_);
    
    // Deallocating nullptr should be safe
    obj_strategy.deallocate(nullptr);
}

// Test custom alignment for BlockStrategy
TEST_F(BumpPoolStrategyTest, CustomAlignmentBlockStrategy) {
    // Create strategy with 64-byte alignment
    constexpr size_t block_size = 128;
    constexpr size_t alignment = 64;
    ftl::allocator::BumpPoolBlockStrategy strategy(*bump_allocator_, block_size, alignment);
    
    // Allocate multiple blocks and verify alignment
    std::vector<void*> ptrs;
    for (int i = 0; i < 5; ++i) {
        void* ptr = strategy.allocate();
        ASSERT_NE(ptr, nullptr);
        
        // Verify alignment
        auto addr = reinterpret_cast<std::uintptr_t>(ptr);
        EXPECT_EQ(addr % alignment, 0) << "Block " << i << " not aligned to " << alignment << " bytes";
        
        ptrs.push_back(ptr);
    }
    
    // Cleanup
    for (void* ptr : ptrs) {
        strategy.deallocate(ptr);
    }
}

// Test cache-line aligned object allocation
TEST_F(BumpPoolStrategyTest, CacheLineAlignedObjectStrategy) {
    // Common cache line size
    constexpr size_t CACHE_LINE_SIZE = 64;
    
    // Create strategy for TestObject with cache-line alignment
    ftl::allocator::BumpPoolObjStrategy<TestObject> obj_strategy(*bump_allocator_, CACHE_LINE_SIZE);
    
    // Allocate multiple objects
    std::vector<TestObject*> objects;
    for (int i = 0; i < 5; ++i) {
        void* raw = obj_strategy.allocate();
        ASSERT_NE(raw, nullptr);
        
        // Verify cache-line alignment
        auto addr = reinterpret_cast<std::uintptr_t>(raw);
        EXPECT_EQ(addr % CACHE_LINE_SIZE, 0) << "Object " << i << " not cache-line aligned";
        
        // Construct object
        TestObject* obj = new (raw) TestObject(i * 100);
        objects.push_back(obj);
    }
    
    // Verify objects work correctly
    for (size_t i = 0; i < objects.size(); ++i) {
        EXPECT_EQ(objects[i]->value, static_cast<int>(i * 100));
    }
    
    // Cleanup
    for (TestObject* obj : objects) {
        obj->~TestObject();
        obj_strategy.deallocate(obj);
    }
}

// Test exhaustion and recovery
TEST_F(BumpPoolStrategyTest, ExhaustionAndRecovery) {
    // Use a small arena to test exhaustion
    constexpr size_t small_arena_size = 512;
    alignas(64) uint8_t small_arena[small_arena_size];
    ftl::BumpAllocator small_bump(small_arena, small_arena_size);
    
    constexpr size_t block_size = 64;
    ftl::allocator::BumpPoolBlockStrategy strategy(small_bump, block_size);
    
    std::vector<void*> ptrs;
    
    // Allocate until exhausted
    while (true) {
        void* ptr = strategy.allocate();
        if (!ptr) break;
        ptrs.push_back(ptr);
    }
    
    EXPECT_GT(ptrs.size(), 0);
    size_t max_allocs = ptrs.size();
    
    // Should not be able to allocate more
    EXPECT_EQ(strategy.allocate(), nullptr);
    
    // Free all
    for (void* ptr : ptrs) {
        strategy.deallocate(ptr);
    }
    
    // Should be able to allocate again (from free list)
    for (size_t i = 0; i < max_allocs; ++i) {
        void* ptr = strategy.allocate();
        ASSERT_NE(ptr, nullptr);
        ptrs[i] = ptr;
    }
    
    // Cleanup
    for (void* ptr : ptrs) {
        strategy.deallocate(ptr);
    }
}