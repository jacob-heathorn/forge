#include "ftl/allocator/unique_obj_allocator.hpp"
#include "ftl/allocator/malloc_strategy.hpp"
#include "ftl/allocator/bump_pool_strategy.hpp"
#include "ftl/bump_allocator.hpp"
#include "gtest/gtest.h"
#include <memory>
#include <vector>

// Test classes for polymorphism
class Base {
public:
    virtual ~Base() { destruction_count++; }
    virtual int getValue() const { return value_; }
    int value_ = 42;
    static int destruction_count;
};

int Base::destruction_count = 0;

class Derived : public Base {
public:
    Derived(int v) { value_ = v; }
    int getValue() const override { return value_ * 2; }
};

// Simple test struct
struct TestObject {
    int x;
    double y;
    std::string z;
    
    TestObject(int a, double b, const std::string& c) 
        : x(a), y(b), z(c) {}
    
    static int destruction_count;
    ~TestObject() { destruction_count++; }
};

int TestObject::destruction_count = 0;

class UniqueObjAllocatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        Base::destruction_count = 0;
        TestObject::destruction_count = 0;
    }
};

// Test basic allocation and automatic deallocation with MallocObjStrategy
TEST_F(UniqueObjAllocatorTest, BasicAllocationWithMalloc) {
    ftl::allocator::MallocObjStrategy<TestObject> strategy;
    ftl::allocator::UniqueObjAllocator<TestObject> allocator(strategy);
    
    {
        auto obj = allocator.acquire(10, 3.14, "test");
        ASSERT_NE(obj, nullptr);
        EXPECT_EQ(obj->x, 10);
        EXPECT_DOUBLE_EQ(obj->y, 3.14);
        EXPECT_EQ(obj->z, "test");
    }
    // obj goes out of scope, should be automatically deallocated
    EXPECT_EQ(TestObject::destruction_count, 1);
}

// Test polymorphic usage with base and derived types
TEST_F(UniqueObjAllocatorTest, PolymorphicUsage) {
    ftl::allocator::MallocObjStrategy<Derived> strategy;
    ftl::allocator::UniqueObjAllocator<Derived, Base> allocator(strategy);
    
    {
        std::unique_ptr<Base, DelegatingDeleter<Base>> base_ptr = allocator.acquire(100);
        ASSERT_NE(base_ptr, nullptr);
        EXPECT_EQ(base_ptr->getValue(), 200);  // Derived::getValue() returns value * 2
    }
    // Should properly destruct through virtual destructor
    EXPECT_EQ(Base::destruction_count, 1);
}

// Test with BumpPoolObjStrategy
TEST_F(UniqueObjAllocatorTest, WithBumpPool) {
    constexpr size_t POOL_SIZE = 4096;
    uint8_t buffer[POOL_SIZE];
    ftl::BumpAllocator bump_alloc(buffer, POOL_SIZE);
    ftl::allocator::BumpPoolObjStrategy<TestObject> strategy(bump_alloc);
    ftl::allocator::UniqueObjAllocator<TestObject> allocator(strategy);
    
    std::vector<std::unique_ptr<TestObject, DelegatingDeleter<TestObject>>> objects;
    
    // Allocate multiple objects
    for (int i = 0; i < 5; ++i) {
        auto obj = allocator.acquire(i, i * 1.1, "item" + std::to_string(i));
        ASSERT_NE(obj, nullptr);
        objects.push_back(std::move(obj));
    }
    
    EXPECT_EQ(TestObject::destruction_count, 0);
    
    // Clear vector, should trigger all destructors
    objects.clear();
    EXPECT_EQ(TestObject::destruction_count, 5);
}

// Test move semantics of unique_ptr
TEST_F(UniqueObjAllocatorTest, MoveSemantics) {
    ftl::allocator::MallocObjStrategy<TestObject> strategy;
    ftl::allocator::UniqueObjAllocator<TestObject> allocator(strategy);
    
    auto obj1 = allocator.acquire(1, 1.0, "first");
    ASSERT_NE(obj1, nullptr);
    
    // Move to another unique_ptr
    auto obj2 = std::move(obj1);
    EXPECT_EQ(obj1, nullptr);
    ASSERT_NE(obj2, nullptr);
    EXPECT_EQ(obj2->x, 1);
    
    // Move assignment
    auto obj3 = allocator.acquire(3, 3.0, "third");
    obj2 = std::move(obj3);
    EXPECT_EQ(obj3, nullptr);
    ASSERT_NE(obj2, nullptr);
    EXPECT_EQ(obj2->x, 3);
    
    // First object should have been destroyed during move assignment
    EXPECT_EQ(TestObject::destruction_count, 1);
}

// Test nullptr handling
TEST_F(UniqueObjAllocatorTest, NullptrHandling) {
    ftl::allocator::MallocObjStrategy<TestObject> strategy;
    ftl::allocator::UniqueObjAllocator<TestObject> allocator(strategy);
    
    // Create empty unique_ptr
    std::unique_ptr<TestObject, DelegatingDeleter<TestObject>> empty_ptr;
    EXPECT_EQ(empty_ptr, nullptr);
    
    // Reset to nullptr
    auto obj = allocator.acquire(1, 1.0, "test");
    ASSERT_NE(obj, nullptr);
    obj.reset();
    EXPECT_EQ(obj, nullptr);
    EXPECT_EQ(TestObject::destruction_count, 1);
}

// Test multiple allocators with same strategy
TEST_F(UniqueObjAllocatorTest, MultipleAllocatorssSameStrategy) {
    ftl::allocator::MallocObjStrategy<TestObject> strategy;
    ftl::allocator::UniqueObjAllocator<TestObject> allocator1(strategy);
    ftl::allocator::UniqueObjAllocator<TestObject> allocator2(strategy);
    
    auto obj1 = allocator1.acquire(1, 1.0, "from_alloc1");
    auto obj2 = allocator2.acquire(2, 2.0, "from_alloc2");
    
    ASSERT_NE(obj1, nullptr);
    ASSERT_NE(obj2, nullptr);
    EXPECT_EQ(obj1->x, 1);
    EXPECT_EQ(obj2->x, 2);
}

// Test with complex polymorphic hierarchy
class Animal {
public:
    virtual ~Animal() = default;
    virtual std::string speak() const = 0;
};

class Dog : public Animal {
public:
    std::string speak() const override { return "Woof!"; }
};

class Cat : public Animal {
public:
    std::string speak() const override { return "Meow!"; }
};

TEST_F(UniqueObjAllocatorTest, PolymorphicCollection) {
    ftl::allocator::MallocObjStrategy<Dog> dog_strategy;
    ftl::allocator::MallocObjStrategy<Cat> cat_strategy;
    ftl::allocator::UniqueObjAllocator<Dog, Animal> dog_allocator(dog_strategy);
    ftl::allocator::UniqueObjAllocator<Cat, Animal> cat_allocator(cat_strategy);
    
    std::vector<std::unique_ptr<Animal, DelegatingDeleter<Animal>>> animals;
    
    animals.push_back(dog_allocator.acquire());
    animals.push_back(cat_allocator.acquire());
    animals.push_back(dog_allocator.acquire());
    
    EXPECT_EQ(animals[0]->speak(), "Woof!");
    EXPECT_EQ(animals[1]->speak(), "Meow!");
    EXPECT_EQ(animals[2]->speak(), "Woof!");
}

// Test release and manual management
TEST_F(UniqueObjAllocatorTest, ReleaseAndManualManagement) {
    ftl::allocator::MallocObjStrategy<TestObject> strategy;
    ftl::allocator::UniqueObjAllocator<TestObject> allocator(strategy);
    
    auto obj = allocator.acquire(5, 5.5, "test");
    ASSERT_NE(obj, nullptr);
    
    // Release ownership
    TestObject* raw_ptr = obj.release();
    EXPECT_EQ(obj, nullptr);
    ASSERT_NE(raw_ptr, nullptr);
    EXPECT_EQ(raw_ptr->x, 5);
    
    // Manual cleanup using allocator
    allocator.deallocate(raw_ptr);
    EXPECT_EQ(TestObject::destruction_count, 1);
}