#pragma once

// Abstract base class for polymorphic deletion of objects.
// Uses void* for complete type erasure, allowing any type to use the same deleter.
// Copying and moving are disabled to enforce unique ownership semantics.
class PolymorphicDeleter
{
public:
  // Default constructor.
  PolymorphicDeleter() = default;

  // Virtual default destructor.
  virtual ~PolymorphicDeleter() = default;

  // No copying or moving.
  PolymorphicDeleter(const PolymorphicDeleter&) = delete;            // Delete copy constructor
  PolymorphicDeleter& operator=(const PolymorphicDeleter&) = delete; // Delete copy assignment operator
  PolymorphicDeleter(PolymorphicDeleter&&) = delete;                 // Delete move constructor
  PolymorphicDeleter& operator=(PolymorphicDeleter&&) = delete;      // Delete move assignment operator

  // Pure virtual call operator to delete object.
  // @param ptr Pointer to object to delete (as void* for type erasure).
  virtual void operator()(void* ptr) = 0;
};


// A default polymorphic deleter that just calls delete on the object.
template <typename T>
class DefaultDeleter final : public PolymorphicDeleter
{
public:
  void operator()(void* ptr) override
  {
    delete static_cast<T*>(ptr);
  }
};


// Delegating deleter that forwards deletion to a PolymorphicDeleter instance.
// 
// Why we need this two-level design:
// 1. std::unique_ptr requires its deleter to be a concrete type (not abstract) that can be 
//    instantiated and stored by value inside the unique_ptr.
// 2. Different allocators (e.g., ObjAllocator<Dog> vs ObjAllocator<Cat>) need different
//    deletion logic, but we want them to produce unique_ptrs with the SAME deleter type
//    so they can be stored in the same container or returned from the same interface.
// 3. PolymorphicDeleter provides runtime polymorphism for different deletion strategies,
//    but it's abstract and can't be used directly as a unique_ptr deleter.
// 4. DelegatingDeleter is a concrete class that can be stored in unique_ptr, and it
//    delegates to a PolymorphicDeleter pointer for the actual deletion logic.
//
// This enables polymorphic factories and collections:
// - All unique_ptr<Animal, DelegatingDeleter<Animal>> have the same type
// - But they can point to Dogs, Cats, etc. with appropriate deletion logic
// - Different concrete types can be stored in the same vector or returned from same interface
template <typename T>
class DelegatingDeleter
{
public:
  // Construct with a pointer to a PolymorphicDeleter.
  // @param deleter Pointer to the polymorphic deleter instance.
  explicit DelegatingDeleter(PolymorphicDeleter* const deleter)
    : polymorphic_deleter_{deleter}
  {}

  // Default constructor, does nothing.
  DelegatingDeleter() = default;

  // Default destructor.
  ~DelegatingDeleter() = default;

  // Default copy and move.
  DelegatingDeleter(const DelegatingDeleter&) = default;
  DelegatingDeleter(DelegatingDeleter&&) noexcept = default;
  DelegatingDeleter& operator=(const DelegatingDeleter&) = default;
  DelegatingDeleter& operator=(DelegatingDeleter&&) = default;

  // Call operator forwards deletion to the underlying polymorphic deleter.
  // @param ptr Pointer to object to delete.
  void operator()(T* ptr) const
  {
    if (polymorphic_deleter_ != nullptr) {
      polymorphic_deleter_->operator()(static_cast<void*>(ptr));
    }
  }

private:
  // Pointer to the polymorphic deleter instance.
  PolymorphicDeleter* polymorphic_deleter_ = nullptr;
};
