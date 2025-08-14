#include "ftl/allocator/obj_allocator.hpp"
#include "ftl/allocator/malloc_obj_strategy.hpp"
#include "ftl/allocator/bump_pool_obj_strategy.hpp"
#include "ftl/allocator/bump_allocator.hpp"
#include "ftl/memory.hpp"
#include "gtest/gtest.h"
#include <memory>
#include <vector>

namespace {

// Test class that tracks construction/destruction
class TrackedObject {
public:
    static int alive_count;
    static int total_constructed;
    
    explicit TrackedObject(int value = 42) : value_(value) {
        ++alive_count;
        ++total_constructed;
    }
    
    TrackedObject(const TrackedObject& other) : value_(other.value_) {
        ++alive_count;
        ++total_constructed;
    }
    
    ~TrackedObject() {
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

int TrackedObject::alive_count = 0;
int TrackedObject::total_constructed = 0;

// Aligned test object
struct AlignedObject {
    alignas(32) uint64_t data[4];
    explicit AlignedObject(uint64_t v = 0) {
        for (auto& d : data) d = v;
    }
};

}  // namespace

class ObjAllocatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        TrackedObject::reset_counts();
    }
    
    void TearDown() override {
        // Verify no leaks
        EXPECT_EQ(TrackedObject::alive_count, 0) << "Memory leak detected";
    }
};

// Test ObjAllocator with MallocObjStrategy
TEST_F(ObjAllocatorTest, MallocStrategyBasic) {
    ftl::allocator::MallocObjStrategy<TrackedObject> strategy;
    ftl::allocator::ObjAllocator<TrackedObject> allocator(strategy);
    
    // Allocate and construct object
    TrackedObject* obj = allocator.allocate(123);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->value(), 123);
    EXPECT_EQ(TrackedObject::alive_count, 1);
    EXPECT_EQ(TrackedObject::total_constructed, 1);
    
    // Deallocate - should destruct object
    allocator.deallocate(obj);
    EXPECT_EQ(TrackedObject::alive_count, 0);
}

// Test ObjAllocator with BumpPoolObjStrategy
TEST_F(ObjAllocatorTest, BumpPoolStrategyBasic) {
    alignas(64) uint8_t arena[4096];
    ftl::BumpAllocator bump(arena, sizeof(arena));
    ftl::allocator::BumpPoolObjStrategy<TrackedObject> strategy(bump);
    ftl::allocator::ObjAllocator<TrackedObject> allocator(strategy);
    
    // Allocate and construct object
    TrackedObject* obj = allocator.allocate(456);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->value(), 456);
    EXPECT_EQ(TrackedObject::alive_count, 1);
    
    // Deallocate - should destruct object and return to free list
    allocator.deallocate(obj);
    EXPECT_EQ(TrackedObject::alive_count, 0);
    
    // Allocate again - should reuse memory from free list
    TrackedObject* obj2 = allocator.allocate(789);
    ASSERT_NE(obj2, nullptr);
    EXPECT_EQ(obj, obj2);  // Should get same memory back
    EXPECT_EQ(obj2->value(), 789);
    
    allocator.deallocate(obj2);
}

// Test multiple allocations and deallocations
TEST_F(ObjAllocatorTest, MultipleAllocations) {
    ftl::allocator::MallocObjStrategy<TrackedObject> strategy;
    ftl::allocator::ObjAllocator<TrackedObject> allocator(strategy);
    
    std::vector<TrackedObject*> objects;
    constexpr int num_objects = 10;
    
    // Allocate multiple objects
    for (int i = 0; i < num_objects; ++i) {
        TrackedObject* obj = allocator.allocate(i * 100);
        ASSERT_NE(obj, nullptr);
        EXPECT_EQ(obj->value(), i * 100);
        objects.push_back(obj);
    }
    
    EXPECT_EQ(TrackedObject::alive_count, num_objects);
    EXPECT_EQ(TrackedObject::total_constructed, num_objects);
    
    // Deallocate all objects
    for (TrackedObject* obj : objects) {
        allocator.deallocate(obj);
    }
    
    EXPECT_EQ(TrackedObject::alive_count, 0);
}

// Test allocating with copy constructor argument
TEST_F(ObjAllocatorTest, AllocateWithCopyConstructorArgument) {
    ftl::allocator::MallocObjStrategy<TrackedObject> strategy;
    ftl::allocator::ObjAllocator<TrackedObject> allocator(strategy);
    
    TrackedObject original(999);
    EXPECT_EQ(TrackedObject::alive_count, 1);
    
    // Allocate with copy construction
    TrackedObject* copy = allocator.allocate(original);
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->value(), 999);
    EXPECT_EQ(TrackedObject::alive_count, 2);
    EXPECT_EQ(TrackedObject::total_constructed, 2);
    
    allocator.deallocate(copy);
    EXPECT_EQ(TrackedObject::alive_count, 1);  // Original still alive
}

// Test null pointer handling
TEST_F(ObjAllocatorTest, NullPointerHandling) {
    ftl::allocator::MallocObjStrategy<TrackedObject> strategy;
    ftl::allocator::ObjAllocator<TrackedObject> allocator(strategy);
    
    // Deallocating nullptr should be safe
    allocator.deallocate(nullptr);
    EXPECT_EQ(TrackedObject::alive_count, 0);
}

// Test aligned object allocation
TEST_F(ObjAllocatorTest, AlignedObjectAllocation) {
    ftl::allocator::MallocObjStrategy<AlignedObject> strategy;
    ftl::allocator::ObjAllocator<AlignedObject> allocator(strategy);
    
    AlignedObject* obj = allocator.allocate(0xDEADBEEF);
    ASSERT_NE(obj, nullptr);
    
    // Verify alignment
    auto addr = reinterpret_cast<std::uintptr_t>(obj);
    EXPECT_EQ(addr % alignof(AlignedObject), 0) << "Object not properly aligned";
    
    // Verify data
    for (const auto& d : obj->data) {
        EXPECT_EQ(d, 0xDEADBEEF);
    }
    
    allocator.deallocate(obj);
}

// Test with default construction (no arguments)
TEST_F(ObjAllocatorTest, DefaultConstruction) {
    ftl::allocator::MallocObjStrategy<TrackedObject> strategy;
    ftl::allocator::ObjAllocator<TrackedObject> allocator(strategy);
    
    TrackedObject* obj = allocator.allocate();  // Default construction
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->value(), 42);  // Default value
    EXPECT_EQ(TrackedObject::alive_count, 1);
    
    allocator.deallocate(obj);
}

// Test exhaustion with bump pool
TEST_F(ObjAllocatorTest, BumpPoolExhaustion) {
    // Small arena to test exhaustion
    alignas(64) uint8_t small_arena[512];
    ftl::BumpAllocator bump(small_arena, sizeof(small_arena));
    ftl::allocator::BumpPoolObjStrategy<TrackedObject> strategy(bump);
    ftl::allocator::ObjAllocator<TrackedObject> allocator(strategy);
    
    std::vector<TrackedObject*> objects;
    
    // Allocate until exhausted
    while (true) {
        TrackedObject* obj = allocator.allocate(objects.size());
        if (!obj) break;
        objects.push_back(obj);
    }
    
    EXPECT_GT(objects.size(), 0u);
    EXPECT_EQ(TrackedObject::alive_count, static_cast<int>(objects.size()));
    
    // Free all objects
    for (TrackedObject* obj : objects) {
        allocator.deallocate(obj);
    }
    
    EXPECT_EQ(TrackedObject::alive_count, 0);
    
    // Should be able to allocate again from free list
    for (size_t i = 0; i < objects.size(); ++i) {
        TrackedObject* obj = allocator.allocate(i);
        ASSERT_NE(obj, nullptr) << "Failed to reallocate from free list";
        objects[i] = obj;
    }
    
    // Cleanup
    for (TrackedObject* obj : objects) {
        allocator.deallocate(obj);
    }
}

// Test make_unique basic functionality
TEST_F(ObjAllocatorTest, MakeUniqueBasic) {
    ftl::allocator::MallocObjStrategy<TrackedObject> strategy;
    ftl::allocator::ObjAllocator<TrackedObject> allocator(strategy);
    
    {
        auto obj = allocator.make_unique(999);
        ASSERT_NE(obj, nullptr);
        EXPECT_EQ(obj->value(), 999);
        EXPECT_EQ(TrackedObject::alive_count, 1);
    }
    // obj goes out of scope, should be automatically deallocated
    EXPECT_EQ(TrackedObject::alive_count, 0);
}

// Helper classes for polymorphic testing
struct TestBase {
    virtual ~TestBase() { destruction_count++; }
    virtual int getValue() const { return 42; }
    static int destruction_count;
};
int TestBase::destruction_count = 0;

struct TestDerived : TestBase {
    int getValue() const override { return 84; }
    explicit TestDerived(int) {}  // Constructor with argument
};

// Test make_unique with polymorphic types
TEST_F(ObjAllocatorTest, MakeUniquePolymorphic) {
    TestBase::destruction_count = 0;
    
    ftl::allocator::MallocObjStrategy<TestDerived> strategy;
    ftl::allocator::ObjAllocator<TestDerived> allocator(strategy);
    
    {
        ftl::unique_ptr<TestBase> base_ptr = allocator.make_unique<TestBase>(100);
        ASSERT_NE(base_ptr, nullptr);
        EXPECT_EQ(base_ptr->getValue(), 84);  // TestDerived::getValue()
    }
    // Should properly destruct through virtual destructor
    EXPECT_EQ(TestBase::destruction_count, 1);
}

// Test make_unique with multiple unique_ptrs
TEST_F(ObjAllocatorTest, MakeUniqueMultiple) {
    ftl::allocator::MallocObjStrategy<TrackedObject> strategy;
    ftl::allocator::ObjAllocator<TrackedObject> allocator(strategy);
    
    std::vector<ftl::unique_ptr<TrackedObject>> objects;
    
    // Create multiple unique_ptrs
    for (int i = 0; i < 5; ++i) {
        objects.push_back(allocator.make_unique(i * 100));
        ASSERT_NE(objects.back(), nullptr);
        EXPECT_EQ(objects.back()->value(), i * 100);
    }
    
    EXPECT_EQ(TrackedObject::alive_count, 5);
    
    // Clear vector, should trigger all destructors
    objects.clear();
    EXPECT_EQ(TrackedObject::alive_count, 0);
}

// Test make_unique move semantics
TEST_F(ObjAllocatorTest, MakeUniqueMoveSemantics) {
    ftl::allocator::MallocObjStrategy<TrackedObject> strategy;
    ftl::allocator::ObjAllocator<TrackedObject> allocator(strategy);
    
    auto obj1 = allocator.make_unique(111);
    ASSERT_NE(obj1, nullptr);
    EXPECT_EQ(TrackedObject::alive_count, 1);
    
    // Move to another unique_ptr
    auto obj2 = std::move(obj1);
    EXPECT_EQ(obj1, nullptr);
    ASSERT_NE(obj2, nullptr);
    EXPECT_EQ(obj2->value(), 111);
    EXPECT_EQ(TrackedObject::alive_count, 1);
    
    // Move assignment
    auto obj3 = allocator.make_unique(333);
    EXPECT_EQ(TrackedObject::alive_count, 2);
    
    obj2 = std::move(obj3);
    EXPECT_EQ(obj3, nullptr);
    ASSERT_NE(obj2, nullptr);
    EXPECT_EQ(obj2->value(), 333);
    EXPECT_EQ(TrackedObject::alive_count, 1);  // First object destroyed
}

// Helper classes for interface pattern test
namespace {
    class IAnimal {
    public:
        virtual ~IAnimal() { destruction_count++; }
        virtual const char* speak() const = 0;
        static int destruction_count;
    };
    int IAnimal::destruction_count = 0;
    
    class Dog : public IAnimal {
    public:
        const char* speak() const override { return "Woof"; }
    };
    
    class Cat : public IAnimal {
    public:
        const char* speak() const override { return "Meow"; }
    };
    
    // Factory interface that returns the SAME unique_ptr type regardless of concrete type
    class IAnimalFactory {
    public:
        virtual ~IAnimalFactory() = default;
        virtual ftl::unique_ptr<IAnimal> createAnimal() = 0;
    };
    
    // Dog factory implementation
    class DogFactory : public IAnimalFactory {
        ftl::allocator::MallocObjStrategy<Dog> strategy_;
        ftl::allocator::ObjAllocator<Dog> allocator_;
    public:
        DogFactory() : allocator_(strategy_) {}
        
        ftl::unique_ptr<IAnimal> createAnimal() override {
            // Returns same type as CatFactory despite using different allocator
            return allocator_.make_unique<IAnimal>();
        }
    };
    
    // Cat factory implementation  
    class CatFactory : public IAnimalFactory {
        ftl::allocator::MallocObjStrategy<Cat> strategy_;
        ftl::allocator::ObjAllocator<Cat> allocator_;
    public:
        CatFactory() : allocator_(strategy_) {}
        
        ftl::unique_ptr<IAnimal> createAnimal() override {
            // Returns same type as DogFactory despite using different allocator
            return allocator_.make_unique<IAnimal>();
        }
    };
}

// Test interface pattern with custom unique_ptr
TEST_F(ObjAllocatorTest, InterfaceWithCustomUniquePtr) {
    IAnimal::destruction_count = 0;
    
    // Use factories through the interface
    DogFactory dogFactory;
    CatFactory catFactory;
    
    // Both factories can be used through the same interface
    IAnimalFactory* factory1 = &dogFactory;
    IAnimalFactory* factory2 = &catFactory;
    
    // Collection of animals from different factories - all have same unique_ptr type
    std::vector<ftl::unique_ptr<IAnimal>> zoo;
    
    // Add animals from different factories
    zoo.push_back(factory1->createAnimal());  // Dog
    zoo.push_back(factory2->createAnimal());  // Cat  
    zoo.push_back(factory1->createAnimal());  // Dog
    
    // Verify correct polymorphic behavior
    EXPECT_STREQ(zoo[0]->speak(), "Woof");
    EXPECT_STREQ(zoo[1]->speak(), "Meow");
    EXPECT_STREQ(zoo[2]->speak(), "Woof");
    
    // Clear the zoo - each animal should be properly destructed using its allocator
    zoo.clear();
    EXPECT_EQ(IAnimal::destruction_count, 3);
    
    // Create a function that accepts animals from any factory
    auto processAnimal = [](ftl::unique_ptr<IAnimal> animal) {
        return std::string(animal->speak());
    };
    
    // Both factories produce compatible types for the function
    EXPECT_EQ(processAnimal(factory1->createAnimal()), "Woof");
    EXPECT_EQ(processAnimal(factory2->createAnimal()), "Meow");
    
    EXPECT_EQ(IAnimal::destruction_count, 5);  // 3 from zoo + 2 from processAnimal
}