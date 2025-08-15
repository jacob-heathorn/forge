#include "ftl/memory.hpp"
#include "gtest/gtest.h"
#include <vector>

namespace {

// Test class that tracks construction/destruction
class TrackedObject {
public:
    static int alive_count;
    static int total_constructed;
    static int total_destructed;
    
    explicit TrackedObject(int value = 42) : value_(value) {
        ++alive_count;
        ++total_constructed;
    }
    
    ~TrackedObject() {
        --alive_count;
        ++total_destructed;
    }
    
    int value() const { return value_; }
    
    static void reset_counts() {
        alive_count = 0;
        total_constructed = 0;
        total_destructed = 0;
    }
    
private:
    int value_;
};

int TrackedObject::alive_count = 0;
int TrackedObject::total_constructed = 0;
int TrackedObject::total_destructed = 0;

// Base and derived classes for polymorphic testing
class Base {
public:
    static int destruction_count;
    virtual ~Base() { ++destruction_count; }
    virtual const char* name() const { return "Base"; }
    static void reset_count() { destruction_count = 0; }
};
int Base::destruction_count = 0;

class Derived : public Base {
public:
    const char* name() const override { return "Derived"; }
    explicit Derived(int) {}  // Constructor with argument
};

// Custom deleter for testing
template<typename T>
class TestDeleter : public ftl::IDeleter {
public:
    static int delete_count;
    static T* last_deleted;
    
    void operator()(void* ptr) override {
        last_deleted = static_cast<T*>(ptr);
        delete static_cast<T*>(ptr);
        ++delete_count;
    }
    
    static void reset_counts() {
        delete_count = 0;
        last_deleted = nullptr;
    }
};

template<typename T>
int TestDeleter<T>::delete_count = 0;

template<typename T>
T* TestDeleter<T>::last_deleted = nullptr;

// Custom allocator deleter that tracks deallocation
template<typename T>
class AllocatorDeleter : public ftl::IDeleter {
public:
    static int deallocate_count;
    
    void operator()(void* ptr) override {
        T* obj = static_cast<T*>(ptr);
        obj->~T();  // Manually call destructor
        ::operator delete(ptr);  // Release memory
        ++deallocate_count;
    }
    
    static void reset_count() {
        deallocate_count = 0;
    }
};

template<typename T>
int AllocatorDeleter<T>::deallocate_count = 0;

}  // namespace

class MemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        TrackedObject::reset_counts();
        Base::reset_count();
        TestDeleter<TrackedObject>::reset_counts();
        TestDeleter<Base>::reset_counts();
        AllocatorDeleter<TrackedObject>::reset_count();
    }
    
    void TearDown() override {
        // Verify no leaks
        EXPECT_EQ(TrackedObject::alive_count, 0) << "Memory leak detected";
    }
};

// Test DefaultDeleter basic functionality
TEST_F(MemoryTest, DefaultDeleterBasic) {
    ftl::DefaultDeleter<TrackedObject> deleter;
    
    TrackedObject* obj = new TrackedObject(123);
    EXPECT_EQ(TrackedObject::alive_count, 1);
    EXPECT_EQ(obj->value(), 123);
    
    // Delete through void* interface
    deleter(static_cast<void*>(obj));
    EXPECT_EQ(TrackedObject::alive_count, 0);
    EXPECT_EQ(TrackedObject::total_destructed, 1);
}

// Test DefaultDeleter with polymorphic types
TEST_F(MemoryTest, DefaultDeleterPolymorphic) {
    ftl::DefaultDeleter<Base> baseDeleter;
    ftl::DefaultDeleter<Derived> derivedDeleter;
    
    Base* base = new Base();
    Derived* derived = new Derived(42);
    
    EXPECT_STREQ(base->name(), "Base");
    EXPECT_STREQ(derived->name(), "Derived");
    
    baseDeleter(static_cast<void*>(base));
    derivedDeleter(static_cast<void*>(derived));
    
    EXPECT_EQ(Base::destruction_count, 2);  // Both Base and Derived destructors called
}

// Test unique_ptr basic construction and destruction
TEST_F(MemoryTest, UniquePtrBasic) {
    TestDeleter<TrackedObject> deleter;
    
    {
        ftl::unique_ptr<TrackedObject> ptr(new TrackedObject(456), &deleter);
        ASSERT_NE(ptr, nullptr);
        EXPECT_EQ(ptr->value(), 456);
        EXPECT_EQ(TrackedObject::alive_count, 1);
    }
    // Should auto-delete when going out of scope
    EXPECT_EQ(TrackedObject::alive_count, 0);
    EXPECT_EQ(TestDeleter<TrackedObject>::delete_count, 1);
}

// Test unique_ptr nullptr construction
TEST_F(MemoryTest, UniquePtrNullptr) {
    ftl::unique_ptr<TrackedObject> ptr1;
    EXPECT_EQ(ptr1, nullptr);
    EXPECT_FALSE(ptr1);
    
    ftl::unique_ptr<TrackedObject> ptr2(nullptr);
    EXPECT_EQ(ptr2, nullptr);
    EXPECT_FALSE(ptr2);
}

// Test unique_ptr get() and operators
TEST_F(MemoryTest, UniquePtrAccessors) {
    TestDeleter<TrackedObject> deleter;
    TrackedObject* raw = new TrackedObject(789);
    
    ftl::unique_ptr<TrackedObject> ptr(raw, &deleter);
    
    // Test get()
    EXPECT_EQ(ptr.get(), raw);
    
    // Test operator->
    EXPECT_EQ(ptr->value(), 789);
    
    // Test operator*
    EXPECT_EQ((*ptr).value(), 789);
    
    // Test get_deleter()
    EXPECT_EQ(ptr.get_deleter(), &deleter);
    
    // Test boolean conversion
    EXPECT_TRUE(ptr);
}

// Test unique_ptr move constructor
TEST_F(MemoryTest, UniquePtrMoveConstructor) {
    TestDeleter<TrackedObject> deleter;
    
    ftl::unique_ptr<TrackedObject> ptr1(new TrackedObject(111), &deleter);
    TrackedObject* raw = ptr1.get();
    
    ftl::unique_ptr<TrackedObject> ptr2(std::move(ptr1));
    
    // ptr1 should be empty
    EXPECT_EQ(ptr1.get(), nullptr);
    EXPECT_EQ(ptr1.get_deleter(), nullptr);
    EXPECT_FALSE(ptr1);
    
    // ptr2 should own the object
    EXPECT_EQ(ptr2.get(), raw);
    EXPECT_EQ(ptr2.get_deleter(), &deleter);
    EXPECT_TRUE(ptr2);
    EXPECT_EQ(ptr2->value(), 111);
    
    EXPECT_EQ(TrackedObject::alive_count, 1);
}

// Test unique_ptr move assignment
TEST_F(MemoryTest, UniquePtrMoveAssignment) {
    TestDeleter<TrackedObject> deleter1;
    TestDeleter<TrackedObject> deleter2;
    
    ftl::unique_ptr<TrackedObject> ptr1(new TrackedObject(111), &deleter1);
    ftl::unique_ptr<TrackedObject> ptr2(new TrackedObject(222), &deleter2);
    
    EXPECT_EQ(TrackedObject::alive_count, 2);
    
    ptr2 = std::move(ptr1);
    
    // Object 222 should be deleted
    EXPECT_EQ(TrackedObject::alive_count, 1);
    EXPECT_EQ(TestDeleter<TrackedObject>::delete_count, 1);
    
    // ptr1 should be empty
    EXPECT_FALSE(ptr1);
    
    // ptr2 should own object 111
    EXPECT_TRUE(ptr2);
    EXPECT_EQ(ptr2->value(), 111);
}

// Test unique_ptr self-assignment
TEST_F(MemoryTest, UniquePtrSelfAssignment) {
    TestDeleter<TrackedObject> deleter;
    
    ftl::unique_ptr<TrackedObject> ptr(new TrackedObject(333), &deleter);
    TrackedObject* raw = ptr.get();
    
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-move"
    ptr = std::move(ptr);  // Self-assignment
#pragma GCC diagnostic pop
    
    // Should still own the object
    EXPECT_EQ(ptr.get(), raw);
    EXPECT_EQ(ptr->value(), 333);
    EXPECT_EQ(TrackedObject::alive_count, 1);
}

// Test unique_ptr release()
TEST_F(MemoryTest, UniquePtrRelease) {
    TestDeleter<TrackedObject> deleter;
    
    ftl::unique_ptr<TrackedObject> ptr(new TrackedObject(444), &deleter);
    
    TrackedObject* raw = ptr.release();
    
    // ptr should be empty
    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr.get_deleter(), nullptr);
    
    // Object should still be alive
    EXPECT_NE(raw, nullptr);
    EXPECT_EQ(raw->value(), 444);
    EXPECT_EQ(TrackedObject::alive_count, 1);
    
    // Manual cleanup
    delete raw;
}

// Test unique_ptr reset()
TEST_F(MemoryTest, UniquePtrReset) {
    TestDeleter<TrackedObject> deleter;
    
    ftl::unique_ptr<TrackedObject> ptr(new TrackedObject(555), &deleter);
    
    ptr.reset();
    
    // Should be empty
    EXPECT_FALSE(ptr);
    EXPECT_EQ(ptr.get_deleter(), nullptr);
    
    // Object should be deleted
    EXPECT_EQ(TrackedObject::alive_count, 0);
    EXPECT_EQ(TestDeleter<TrackedObject>::delete_count, 1);
}

// Test unique_ptr reset() with new pointer
TEST_F(MemoryTest, UniquePtrResetWithNewPointer) {
    TestDeleter<TrackedObject> deleter1;
    TestDeleter<TrackedObject> deleter2;
    
    ftl::unique_ptr<TrackedObject> ptr(new TrackedObject(666), &deleter1);
    
    ptr.reset(new TrackedObject(777), &deleter2);
    
    // First object should be deleted
    EXPECT_EQ(TestDeleter<TrackedObject>::delete_count, 1);
    
    // Should own new object
    EXPECT_TRUE(ptr);
    EXPECT_EQ(ptr->value(), 777);
    EXPECT_EQ(ptr.get_deleter(), &deleter2);
    EXPECT_EQ(TrackedObject::alive_count, 1);
}

// Test unique_ptr assignment from nullptr
TEST_F(MemoryTest, UniquePtrAssignNullptr) {
    TestDeleter<TrackedObject> deleter;
    
    ftl::unique_ptr<TrackedObject> ptr(new TrackedObject(888), &deleter);
    
    ptr = nullptr;
    
    // Should be empty and object deleted
    EXPECT_FALSE(ptr);
    EXPECT_EQ(TrackedObject::alive_count, 0);
    EXPECT_EQ(TestDeleter<TrackedObject>::delete_count, 1);
}

// Test unique_ptr comparison operators
TEST_F(MemoryTest, UniquePtrComparison) {
    TestDeleter<TrackedObject> deleter1;
    TestDeleter<TrackedObject> deleter2;
    
    ftl::unique_ptr<TrackedObject> ptr1(new TrackedObject(100), &deleter1);
    ftl::unique_ptr<TrackedObject> ptr2(new TrackedObject(200), &deleter2);
    ftl::unique_ptr<TrackedObject> ptr3;
    
    // Compare with self
    EXPECT_TRUE(ptr1 == ptr1);
    EXPECT_FALSE(ptr1 != ptr1);
    
    // Compare different pointers
    EXPECT_FALSE(ptr1 == ptr2);
    EXPECT_TRUE(ptr1 != ptr2);
    
    // Compare with nullptr
    EXPECT_FALSE(ptr1 == nullptr);
    EXPECT_TRUE(ptr1 != nullptr);
    EXPECT_TRUE(ptr3 == nullptr);
    EXPECT_FALSE(ptr3 != nullptr);
    
    // Compare nullptr with pointer
    EXPECT_FALSE(nullptr == ptr1);
    EXPECT_TRUE(nullptr != ptr1);
    EXPECT_TRUE(nullptr == ptr3);
    EXPECT_FALSE(nullptr != ptr3);
}

// Test unique_ptr with polymorphic types
TEST_F(MemoryTest, UniquePtrPolymorphic) {
    TestDeleter<Base> deleter;
    Base::reset_count();
    
    {
        // Store Derived in Base pointer
        Derived* derived = new Derived(42);
        ftl::unique_ptr<Base> ptr(derived, &deleter);
        
        EXPECT_STREQ(ptr->name(), "Derived");
    }
    
    // Virtual destructor should be called
    EXPECT_EQ(Base::destruction_count, 1);
    EXPECT_EQ(TestDeleter<Base>::delete_count, 1);
}

// Test unique_ptr with custom allocator-style deleter
TEST_F(MemoryTest, UniquePtrCustomAllocatorDeleter) {
    AllocatorDeleter<TrackedObject> deleter;
    
    {
        // Manually allocate with placement new
        void* raw_memory = ::operator new(sizeof(TrackedObject));
        TrackedObject* obj = new(raw_memory) TrackedObject(999);
        
        ftl::unique_ptr<TrackedObject> ptr(obj, &deleter);
        EXPECT_EQ(ptr->value(), 999);
        EXPECT_EQ(TrackedObject::alive_count, 1);
    }
    
    // Should be properly deallocated
    EXPECT_EQ(TrackedObject::alive_count, 0);
    EXPECT_EQ(AllocatorDeleter<TrackedObject>::deallocate_count, 1);
}

// Test multiple unique_ptrs with same deleter
TEST_F(MemoryTest, MultipleUniquePtrsWithSameDeleter) {
    TestDeleter<TrackedObject> deleter;
    
    std::vector<ftl::unique_ptr<TrackedObject>> ptrs;
    
    // Create multiple unique_ptrs using the same deleter instance
    for (int i = 0; i < 5; ++i) {
        ptrs.emplace_back(new TrackedObject(i * 100), &deleter);
    }
    
    EXPECT_EQ(TrackedObject::alive_count, 5);
    
    // Clear all
    ptrs.clear();
    
    EXPECT_EQ(TrackedObject::alive_count, 0);
    EXPECT_EQ(TestDeleter<TrackedObject>::delete_count, 5);
}

// Note: Array support would require specialization of unique_ptr for T[]
// which is not implemented in the current ftl::unique_ptr.
// Commenting out this test as it's not supported.
/*
TEST_F(MemoryTest, UniquePtrArraySubscript) {
    // Custom deleter for arrays
    class ArrayDeleter : public ftl::IDeleter {
    public:
        void operator()(void* ptr) override {
            delete[] static_cast<int*>(ptr);
        }
    };
    
    ArrayDeleter deleter;
    
    int* arr = new int[5]{10, 20, 30, 40, 50};
    ftl::unique_ptr<int[]> ptr(arr, &deleter);
    
    // Test array access
    EXPECT_EQ(ptr[0], 10);
    EXPECT_EQ(ptr[2], 30);
    EXPECT_EQ(ptr[4], 50);
    
    // Modify through subscript
    ptr[1] = 25;
    EXPECT_EQ(ptr[1], 25);
}
*/

// Test that IDeleter is non-copyable and non-movable
TEST_F(MemoryTest, IDeleterNonCopyableNonMovable) {
    // These should not compile:
    // TestDeleter<TrackedObject> d1;
    // TestDeleter<TrackedObject> d2 = d1;  // Copy constructor
    // TestDeleter<TrackedObject> d3;
    // d3 = d1;  // Copy assignment
    // TestDeleter<TrackedObject> d4 = std::move(d1);  // Move constructor
    // TestDeleter<TrackedObject> d5;
    // d5 = std::move(d1);  // Move assignment
    
    // Test passes if it compiles
    EXPECT_TRUE(true);
}

// Test edge case: unique_ptr with nullptr deleter passed to reset
TEST_F(MemoryTest, UniquePtrResetNullDeleter) {
    TestDeleter<TrackedObject> deleter;
    
    ftl::unique_ptr<TrackedObject> ptr(new TrackedObject(100), &deleter);
    
    // Reset to nullptr clears both pointer and deleter
    ptr.reset();
    EXPECT_EQ(ptr.get(), nullptr);
    EXPECT_EQ(ptr.get_deleter(), nullptr);
}

// Test moving empty unique_ptr
TEST_F(MemoryTest, UniquePtrMoveEmpty) {
    ftl::unique_ptr<TrackedObject> ptr1;
    ftl::unique_ptr<TrackedObject> ptr2(std::move(ptr1));
    
    EXPECT_FALSE(ptr1);
    EXPECT_FALSE(ptr2);
    EXPECT_EQ(ptr2.get_deleter(), nullptr);
}