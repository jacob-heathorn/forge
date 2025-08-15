#include "ftl/allocator/fixed_pool_obj_strategy.hpp"
#include "ftl/allocator/obj_allocator.hpp"
#include "ftl/allocator/bump_allocator.hpp"
#include "gtest/gtest.h"
#include <vector>
#include <thread>

namespace {

// Test class that tracks construction/destruction
class TestObject {
public:
    static int alive_count;
    static int total_constructed;
    
    explicit TestObject(int value = 42) : value_(value) {
        ++alive_count;
        ++total_constructed;
    }
    
    ~TestObject() {
        --alive_count;
    }
    
    int value() const { return value_; }
    
    static void reset_counts() {
        alive_count = 0;
        total_constructed = 0;
    }
    
private:
    int value_;
    char padding_[60];  // Make object larger
};

int TestObject::alive_count = 0;
int TestObject::total_constructed = 0;

// Small test object
struct SmallObject {
    uint32_t id;
    explicit SmallObject(uint32_t i = 0) : id(i) {}
};

}  // namespace

class FixedPoolObjStrategyTest : public ::testing::Test {
protected:
    void SetUp() override {
        TestObject::reset_counts();
    }
    
    void TearDown() override {
        // Verify no leaks
        EXPECT_EQ(TestObject::alive_count, 0) << "Memory leak detected";
    }
};

// Test basic allocation and deallocation
TEST_F(FixedPoolObjStrategyTest, BasicAllocation) {
    alignas(64) uint8_t arena[4096];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    ftl::allocator::FixedPoolObjStrategy<TestObject> strategy(bump, 10);
    
    // Should have full capacity available
    EXPECT_EQ(strategy.capacity(), 10u);
    EXPECT_EQ(strategy.available(), 10u);
    EXPECT_EQ(strategy.allocated(), 0u);
    
    // Allocate an object
    void* mem = strategy.allocate();
    ASSERT_NE(mem, nullptr);
    
    EXPECT_EQ(strategy.available(), 9u);
    EXPECT_EQ(strategy.allocated(), 1u);
    
    // Construct object in allocated memory
    TestObject* obj = new(mem) TestObject(123);
    EXPECT_EQ(obj->value(), 123);
    EXPECT_EQ(TestObject::alive_count, 1);
    
    // Destruct and deallocate
    obj->~TestObject();
    strategy.deallocate(mem);
    
    EXPECT_EQ(strategy.available(), 10u);
    EXPECT_EQ(strategy.allocated(), 0u);
    EXPECT_EQ(TestObject::alive_count, 0);
}

// Test pool exhaustion
TEST_F(FixedPoolObjStrategyTest, PoolExhaustion) {
    alignas(64) uint8_t arena[4096];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    constexpr std::size_t POOL_SIZE = 5;
    ftl::allocator::FixedPoolObjStrategy<TestObject> strategy(bump, POOL_SIZE);
    
    std::vector<void*> allocated;
    
    // Allocate all objects
    for (std::size_t i = 0; i < POOL_SIZE; ++i) {
        void* mem = strategy.allocate();
        ASSERT_NE(mem, nullptr) << "Failed to allocate object " << i;
        allocated.push_back(mem);
        new(mem) TestObject(i);
    }
    
    EXPECT_EQ(strategy.available(), 0u);
    EXPECT_EQ(strategy.allocated(), POOL_SIZE);
    EXPECT_EQ(TestObject::alive_count, POOL_SIZE);
    
    // Try to allocate one more - should fail
    void* mem = strategy.allocate();
    EXPECT_EQ(mem, nullptr);
    
    // Free one object
    TestObject* obj = static_cast<TestObject*>(allocated[2]);
    obj->~TestObject();
    strategy.deallocate(allocated[2]);
    
    // Now allocation should succeed
    mem = strategy.allocate();
    ASSERT_NE(mem, nullptr);
    new(mem) TestObject(99);
    
    // Cleanup
    obj = static_cast<TestObject*>(mem);
    obj->~TestObject();
    strategy.deallocate(mem);
    
    for (std::size_t i = 0; i < POOL_SIZE; ++i) {
        if (i != 2) {  // Already freed
            obj = static_cast<TestObject*>(allocated[i]);
            obj->~TestObject();
            strategy.deallocate(allocated[i]);
        }
    }
}

// Test with ObjAllocator
TEST_F(FixedPoolObjStrategyTest, WithObjAllocator) {
    alignas(64) uint8_t arena[4096];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    ftl::allocator::FixedPoolObjStrategy<TestObject> strategy(bump, 8);
    ftl::allocator::ObjAllocator<TestObject> allocator(strategy);
    
    // Allocate and construct
    TestObject* obj1 = allocator.allocate(111);
    ASSERT_NE(obj1, nullptr);
    EXPECT_EQ(obj1->value(), 111);
    
    TestObject* obj2 = allocator.allocate(222);
    ASSERT_NE(obj2, nullptr);
    EXPECT_EQ(obj2->value(), 222);
    
    EXPECT_EQ(TestObject::alive_count, 2);
    EXPECT_EQ(strategy.allocated(), 2u);
    
    // Deallocate
    allocator.deallocate(obj1);
    allocator.deallocate(obj2);
    
    EXPECT_EQ(TestObject::alive_count, 0);
    EXPECT_EQ(strategy.available(), 8u);
}

// Test invalid deallocation
TEST_F(FixedPoolObjStrategyTest, InvalidDeallocation) {
    alignas(64) uint8_t arena[4096];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    ftl::allocator::FixedPoolObjStrategy<TestObject> strategy(bump, 5);
    
    // Null pointer should be safe
    strategy.deallocate(nullptr);
    EXPECT_EQ(strategy.available(), 5u);
    
    // Pointer not from pool should be ignored
    TestObject stack_obj;
    strategy.deallocate(&stack_obj);
    EXPECT_EQ(strategy.available(), 5u);
    
    // Allocate and deallocate properly
    void* mem = strategy.allocate();
    ASSERT_NE(mem, nullptr);
    EXPECT_EQ(strategy.available(), 4u);
    
    strategy.deallocate(mem);
    EXPECT_EQ(strategy.available(), 5u);
}

// Test allocation failure when bump allocator is exhausted
TEST_F(FixedPoolObjStrategyTest, BumpAllocatorExhaustion) {
    // Very small arena that can't fit 100 TestObjects
    alignas(64) uint8_t small_arena[128];
    ftl::BumpAllocator bump(small_arena, sizeof(small_arena));
    
    // Try to allocate more than fits - should fail and return 0 capacity
    ftl::allocator::FixedPoolObjStrategy<TestObject> strategy(bump, 100);
    
    // Since we couldn't allocate all 100, capacity should be 0 (all-or-nothing)
    EXPECT_EQ(strategy.capacity(), 0u);
    EXPECT_EQ(strategy.available(), 0u);
    
    // Allocation should fail
    void* mem = strategy.allocate();
    EXPECT_EQ(mem, nullptr);
}

// Test with zero count
TEST_F(FixedPoolObjStrategyTest, ZeroCount) {
    alignas(64) uint8_t arena[4096];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    ftl::allocator::FixedPoolObjStrategy<TestObject> strategy(bump, 0);
    
    EXPECT_EQ(strategy.capacity(), 0u);
    EXPECT_EQ(strategy.available(), 0u);
    
    // Allocation should fail
    void* mem = strategy.allocate();
    EXPECT_EQ(mem, nullptr);
}

// Test multiple strategies from same bump allocator
TEST_F(FixedPoolObjStrategyTest, MultipleStrategiesFromSameBump) {
    alignas(64) uint8_t arena[8192];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    // Create multiple strategies from same bump allocator
    ftl::allocator::FixedPoolObjStrategy<TestObject> strategy1(bump, 5);
    ftl::allocator::FixedPoolObjStrategy<SmallObject> strategy2(bump, 10);
    ftl::allocator::FixedPoolObjStrategy<TestObject> strategy3(bump, 3);
    
    EXPECT_EQ(strategy1.capacity(), 5u);
    EXPECT_EQ(strategy2.capacity(), 10u);
    EXPECT_EQ(strategy3.capacity(), 3u);
    
    // Each should work independently
    void* mem1 = strategy1.allocate();
    ASSERT_NE(mem1, nullptr);
    
    void* mem2 = strategy2.allocate();
    ASSERT_NE(mem2, nullptr);
    
    void* mem3 = strategy3.allocate();
    ASSERT_NE(mem3, nullptr);
    
    // Pointers should be different
    EXPECT_NE(mem1, mem2);
    EXPECT_NE(mem1, mem3);
    EXPECT_NE(mem2, mem3);
    
    // Cleanup
    strategy1.deallocate(mem1);
    strategy2.deallocate(mem2);
    strategy3.deallocate(mem3);
}

// Test thread safety
TEST_F(FixedPoolObjStrategyTest, ThreadSafety) {
    alignas(64) uint8_t arena[32768];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    
    constexpr std::size_t POOL_SIZE = 100;
    constexpr std::size_t NUM_THREADS = 4;
    constexpr std::size_t OPS_PER_THREAD = 1000;
    
    ftl::allocator::FixedPoolObjStrategy<SmallObject> strategy(bump, POOL_SIZE);
    
    auto worker = [&]() {
        std::vector<void*> my_allocs;
        
        for (std::size_t i = 0; i < OPS_PER_THREAD; ++i) {
            // Try to allocate
            void* mem = strategy.allocate();
            if (mem) {
                my_allocs.push_back(mem);
                
                // Keep some allocations for a while
                if (my_allocs.size() > 10) {
                    // Free oldest allocation
                    strategy.deallocate(my_allocs.front());
                    my_allocs.erase(my_allocs.begin());
                }
            } else {
                // If allocation failed, free something if we have it
                if (!my_allocs.empty()) {
                    strategy.deallocate(my_allocs.back());
                    my_allocs.pop_back();
                }
            }
        }
        
        // Free all remaining allocations
        for (void* mem : my_allocs) {
            strategy.deallocate(mem);
        }
    };
    
    // Run threads
    std::vector<std::thread> threads;
    for (std::size_t i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(worker);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // All memory should be freed
    EXPECT_EQ(strategy.available(), POOL_SIZE);
}