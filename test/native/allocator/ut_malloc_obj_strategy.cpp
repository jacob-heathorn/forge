#include "ftl/allocator/strategy.hpp"
#include "ftl/allocator/malloc_obj_strategy.hpp"
#include "gtest/gtest.h"
#include <cstdint>
#include <cstring>
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

}  // namespace

class MallocStrategyTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

// Test object allocation with MallocStrategy
TEST_F(MallocStrategyTest, ObjectAllocationBasic) {
    ftl::allocator::MallocObjStrategy<TestObject> obj_strategy;
    
    // Allocate raw memory for TestObject
    void* raw = obj_strategy.allocate();
    ASSERT_NE(raw, nullptr);
    
    // Construct object using placement new
    TestObject* obj = new (raw) TestObject(123);
    EXPECT_EQ(obj->value, 123);
    EXPECT_TRUE(obj->active);
    
    // Destroy object
    obj->~TestObject();
    
    // Deallocate memory
    obj_strategy.deallocate(raw);
}

// Test ObjectAllocator with MallocStrategy
TEST_F(MallocStrategyTest, ObjectAllocatorIntegration) {
    ftl::allocator::MallocObjStrategy<TestObject> obj_strategy;
    // Note: ObjectAllocator would need to be implemented separately
    // For now, test raw allocation
    
    // Allocate raw memory
    void* raw = obj_strategy.allocate();
    ASSERT_NE(raw, nullptr);
    
    // Construct object
    TestObject* obj = new (raw) TestObject();
    EXPECT_EQ(obj->value, 42);  // Default constructor value
    EXPECT_TRUE(obj->active);
    
    // Modify object
    obj->value = 999;
    EXPECT_EQ(obj->value, 999);
    
    // Destroy and deallocate
    obj->~TestObject();
    obj_strategy.deallocate(raw);
}

// Test multiple object allocations
TEST_F(MallocStrategyTest, MultipleObjectAllocations) {
    ftl::allocator::MallocObjStrategy<TestObject> obj_strategy;
    
    std::vector<TestObject*> objects;
    constexpr size_t num_objects = 20;
    
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
    
    // Deallocate all
    for (TestObject* obj : objects) {
        obj->~TestObject();
        obj_strategy.deallocate(obj);
    }
}

// Test aligned object allocation
TEST_F(MallocStrategyTest, AlignedObjectAllocation) {
    ftl::allocator::MallocObjStrategy<AlignedObject> obj_strategy;
    
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
TEST_F(MallocStrategyTest, NullPointerHandling) {
    ftl::allocator::MallocObjStrategy<TestObject> obj_strategy;
    
    // Deallocating nullptr should be safe
    obj_strategy.deallocate(nullptr);
}

// Test natural alignment for objects
TEST_F(MallocStrategyTest, ObjectNaturalAlignment) {
    // Objects now use their natural alignment only
    ftl::allocator::MallocObjStrategy<TestObject> obj_strategy;
    
    // Allocate multiple objects
    std::vector<TestObject*> objects;
    for (int i = 0; i < 5; ++i) {
        void* raw = obj_strategy.allocate();
        ASSERT_NE(raw, nullptr);
        
        // Verify natural alignment
        auto addr = reinterpret_cast<std::uintptr_t>(raw);
        EXPECT_EQ(addr % alignof(TestObject), 0) << "Object " << i << " not naturally aligned";
        
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