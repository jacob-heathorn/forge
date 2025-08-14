#include "ftl/allocator/strategy.hpp"
#include "ftl/allocator/bump_pool_strategy.hpp"
#include "ftl/allocator/bump_allocator.hpp"
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
    ftl::allocator::BumpPoolObjStrategy<TestObject> obj_strategy(*bump_allocator_);
    
    // Deallocating nullptr should be safe
    obj_strategy.deallocate(nullptr);
}

