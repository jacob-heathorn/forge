#pragma once

// Abstract base class for polymorphic deletion of objects of type T.
// Used as a type-erased deleter with std::unique_ptr.
// Copying and moving are disabled to enforce unique ownership semantics.
template <typename T>
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

  // Pure virtual call operator to delete object of type T.
  // @param ptr Pointer to object to delete.
  virtual void operator()(T* ptr) = 0;
};


// A default polymorphic deleter that just calls delete on the object.
template <typename T>
class DefaultDeleter final : public PolymorphicDeleter<T>
{
public:
  void operator()(T* ptr) override
  {
    delete ptr;
  }
};


// Delegating deleter that owns and forwards deletion to a PolymorphicDeleter<T> instance.
// We store a pointer because PolymorphicDeleter<T> is abstract and cannot be used directly
// as a std::unique_ptr deleter; owning the base-pointer enables runtime polymorphism and
// type erasure of the deletion logic, while satisfying std::unique_ptr's requirement that
// the deleter type be completely known at compile time.
template <typename T>
class DelegatingDeleter
{
public:
  // Construct with a pointer to a PolymorphicDeleter<T>.
  // @param deleter Pointer to the polymorphic deleter instance.
  explicit DelegatingDeleter(PolymorphicDeleter<T>* const deleter)
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
      polymorphic_deleter_->operator()(ptr);
    }
  }

private:
  // Pointer to the polymorphic deleter instance.
  PolymorphicDeleter<T>* polymorphic_deleter_ = nullptr;
};
