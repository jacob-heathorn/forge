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


// Delegating deleter that owns and forwards deletion to a PolymorphicDeleter instance.
// Templated on T for type safety at the unique_ptr level, but uses void* internally.
// We store a pointer because PolymorphicDeleter is abstract and cannot be used directly
// as a std::unique_ptr deleter; owning the base-pointer enables runtime polymorphism and
// type erasure of the deletion logic, while satisfying std::unique_ptr's requirement that
// the deleter type be completely known at compile time.
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
