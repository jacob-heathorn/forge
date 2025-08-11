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

// Test basic buffer allocation with MallocBlockStrategy
TEST_F(MallocStrategyTest, BufferAllocationBasic) {
    constexpr size_t block_size = 256;
    ftl::allocator::MallocBlockStrategy strategy(block_size);
    
    // Allocate a buffer
    void* ptr = strategy.allocate();
    ASSERT_NE(ptr, nullptr);
    
    // Write to the buffer to verify it's valid memory
    std::memset(ptr, 0xAB, block_size);
    
    // Deallocate
    strategy.deallocate(ptr);
}

// Test multiple buffer allocations
TEST_F(MallocStrategyTest, MultipleBufferAllocations) {
    constexpr size_t block_size = 128;
    ftl::allocator::MallocBlockStrategy strategy(block_size);
    
    std::vector<void*> ptrs;
    constexpr size_t num_allocs = 10;
    
    // Allocate multiple buffers
    for (size_t i = 0; i < num_allocs; ++i) {
        void* ptr = strategy.allocate();
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
        strategy.deallocate(ptr);
    }
}

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
    EXPECT_FALSE(obj->active);
    
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


// Test that different block sizes use different state
TEST_F(MallocStrategyTest, DifferentBlockSizesIndependent) {
    ftl::allocator::MallocBlockStrategy strat1(64);
    ftl::allocator::MallocBlockStrategy strat2(128);
    
    void* ptr1 = strat1.allocate();
    void* ptr2 = strat2.allocate();
    
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
    
    strat1.deallocate(ptr1);
    strat2.deallocate(ptr2);
}

// Test null pointer handling
TEST_F(MallocStrategyTest, NullPointerHandling) {
    ftl::allocator::MallocBlockStrategy strategy(64);
    
    // Deallocating nullptr should be safe
    strategy.deallocate(nullptr);
    
    ftl::allocator::MallocObjStrategy<TestObject> obj_strategy;
    
    // Deallocating nullptr should be safe
    obj_strategy.deallocate(nullptr);
}

// Test custom alignment for BlockStrategy
TEST_F(MallocStrategyTest, CustomAlignmentBlockStrategy) {
    // Create strategy with 64-byte alignment
    constexpr size_t block_size = 128;
    constexpr size_t alignment = 64;
    ftl::allocator::MallocBlockStrategy strategy(block_size, alignment);
    
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
TEST_F(MallocStrategyTest, CacheLineAlignedObjectStrategy) {
    // Common cache line size
    constexpr size_t CACHE_LINE_SIZE = 64;
    
    // Create strategy for TestObject with cache-line alignment
    ftl::allocator::MallocObjStrategy<TestObject> obj_strategy(CACHE_LINE_SIZE);
    
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