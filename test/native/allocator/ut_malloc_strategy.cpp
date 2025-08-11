#include "ftl/allocator/strategy.hpp"
#include "ftl/allocator/malloc_strategy.hpp"
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

}  // namespace

class MallocStrategyTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

// Test basic buffer allocation with MallocStrategy
TEST_F(MallocStrategyTest, BufferAllocationBasic) {
    constexpr size_t block_size = 256;
    auto strategy = ftl::allocator::MallocStrategy::make(block_size);
    
    ASSERT_NE(strategy.allocate, nullptr);
    ASSERT_NE(strategy.deallocate, nullptr);
    ASSERT_NE(strategy.self, nullptr);
    
    // Allocate a buffer with default alignment
    void* ptr = strategy.allocate(strategy.self, alignof(std::max_align_t));
    ASSERT_NE(ptr, nullptr);
    
    // Write to the buffer to verify it's valid memory
    std::memset(ptr, 0xAB, block_size);
    
    // Deallocate
    strategy.deallocate(strategy.self, ptr);
}

// Test multiple buffer allocations
TEST_F(MallocStrategyTest, MultipleBufferAllocations) {
    constexpr size_t block_size = 128;
    auto strategy = ftl::allocator::MallocStrategy::make(block_size);
    
    std::vector<void*> ptrs;
    constexpr size_t num_allocs = 10;
    
    // Allocate multiple buffers
    for (size_t i = 0; i < num_allocs; ++i) {
        void* ptr = strategy.allocate(strategy.self, alignof(std::max_align_t));
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
        
        // Write unique pattern to each buffer
        std::memset(ptr, static_cast<uint8_t>(i), block_size);
    }
    
    // Verify all pointers are unique
    for (size_t i = 0; i < num_allocs; ++i) {
        for (size_t j = i + 1; j < num_allocs; ++j) {
            EXPECT_NE(ptrs[i], ptrs[j]);
        }
    }
    
    // Verify patterns are intact
    for (size_t i = 0; i < num_allocs; ++i) {
        uint8_t* buf = static_cast<uint8_t*>(ptrs[i]);
        for (size_t j = 0; j < block_size; ++j) {
            EXPECT_EQ(buf[j], static_cast<uint8_t>(i));
        }
    }
    
    // Deallocate all
    for (void* ptr : ptrs) {
        strategy.deallocate(strategy.self, ptr);
    }
}

// Test object allocation with MallocStrategy
TEST_F(MallocStrategyTest, ObjectAllocationBasic) {
    auto obj_strategy = ftl::allocator::MallocStrategy::make_for<TestObject>();
    
    ASSERT_NE(obj_strategy.allocate, nullptr);
    ASSERT_NE(obj_strategy.deallocate, nullptr);
    
    // Allocate raw memory for TestObject
    void* raw = obj_strategy.allocate(obj_strategy.self, alignof(TestObject));
    ASSERT_NE(raw, nullptr);
    
    // Construct object using placement new
    TestObject* obj = new (raw) TestObject(123);
    EXPECT_EQ(obj->value, 123);
    EXPECT_TRUE(obj->active);
    
    // Destroy object
    obj->~TestObject();
    EXPECT_FALSE(obj->active);
    
    // Deallocate memory
    obj_strategy.deallocate(obj_strategy.self, raw);
}

// Test ObjectAllocator with MallocStrategy
TEST_F(MallocStrategyTest, ObjectAllocatorIntegration) {
    auto obj_strategy = ftl::allocator::MallocStrategy::make_for<TestObject>();
    // Note: ObjectAllocator would need to be implemented separately
    // For now, test raw allocation
    
    // Allocate raw memory
    void* raw = obj_strategy.allocate(obj_strategy.self, alignof(TestObject));
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
    obj_strategy.deallocate(obj_strategy.self, raw);
}

// Test multiple object allocations
TEST_F(MallocStrategyTest, MultipleObjectAllocations) {
    auto obj_strategy = ftl::allocator::MallocStrategy::make_for<TestObject>();
    
    std::vector<TestObject*> objects;
    constexpr size_t num_objects = 20;
    
    // Allocate multiple objects
    for (size_t i = 0; i < num_objects; ++i) {
        void* raw = obj_strategy.allocate(obj_strategy.self, alignof(TestObject));
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
        obj_strategy.deallocate(obj_strategy.self, obj);
    }
}

// Test aligned object allocation
TEST_F(MallocStrategyTest, AlignedObjectAllocation) {
    auto obj_strategy = ftl::allocator::MallocStrategy::make_for<AlignedObject>();
    
    // Allocate aligned object
    void* raw = obj_strategy.allocate(obj_strategy.self, alignof(AlignedObject));
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
    obj_strategy.deallocate(obj_strategy.self, raw);
}


// Test that different block sizes use different state
TEST_F(MallocStrategyTest, DifferentBlockSizesIndependent) {
    auto strat1 = ftl::allocator::MallocStrategy::make(64);
    auto strat2 = ftl::allocator::MallocStrategy::make(128);
    
    void* ptr1 = strat1.allocate(strat1.self, alignof(std::max_align_t));
    void* ptr2 = strat2.allocate(strat2.self, alignof(std::max_align_t));
    
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_NE(ptr1, ptr2);
    
    // Write different patterns
    std::memset(ptr1, 0xAA, 64);
    std::memset(ptr2, 0xBB, 128);
    
    // Verify patterns
    uint8_t* buf1 = static_cast<uint8_t*>(ptr1);
    uint8_t* buf2 = static_cast<uint8_t*>(ptr2);
    
    for (size_t i = 0; i < 64; ++i) {
        EXPECT_EQ(buf1[i], 0xAA);
    }
    
    for (size_t i = 0; i < 128; ++i) {
        EXPECT_EQ(buf2[i], 0xBB);
    }
    
    strat1.deallocate(strat1.self, ptr1);
    strat2.deallocate(strat2.self, ptr2);
}

// Test null pointer handling
TEST_F(MallocStrategyTest, NullPointerHandling) {
    auto strategy = ftl::allocator::MallocStrategy::make(64);
    
    // Deallocating nullptr should be safe
    strategy.deallocate(strategy.self, nullptr);
    
    auto obj_strategy = ftl::allocator::MallocStrategy::make_for<TestObject>();
    
    // Deallocating nullptr should be safe
    obj_strategy.deallocate(obj_strategy.self, nullptr);
}