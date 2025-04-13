#pragma once

#include <cassert>
#include <cstddef>
#include <type_traits>
#include <new>
#include <utility>

namespace ftl {

// A minimal, header-only function wrapper that mimics std::function 
// but stores the callable in an inline buffer (no dynamic memory allocation).
// R(Args...) is the signature, and BufferSize (default 64) is the size of the inline storage.
template <typename Signature, std::size_t BufferSize = 64>
class function;  // Primary template declaration

// Specialization for function signature R(Args...)
template <typename R, typename... Args, std::size_t BufferSize>
class function<R(Args...), BufferSize> {
 public:
  // Default constructor: creates an empty function.
  function() : invoker_(nullptr), copier_(nullptr), deleter_(nullptr) {}

  // Constructor template: accepts any callable that is invocable with Args...
  template <typename T>
  function(const T& f) {
    // Use std::decay to convert f into a concrete type (for example,
    // a function pointer if T is a function type, or a lambda/functor type).
    using Callable = typename std::decay<T>::type;
    static_assert(sizeof(Callable) <= BufferSize,
                  "Callable is too large for ftl::function storage");
    static_assert(alignof(Callable) <= alignof(storage_),
                  "Callable alignment is too strict for ftl::function");

    // Construct the callable in our inline storage using placement new.
    new (&storage_) Callable(f);
    invoker_ = &invoke<Callable>;
    copier_  = &copy<Callable>;
    deleter_ = &destroy<Callable>;
  }

  // Copy constructor.
  function(const function& other)
      : invoker_(nullptr), copier_(nullptr), deleter_(nullptr) {
    if (other) {
      other.copier_(&other.storage_, &storage_);
      invoker_  = other.invoker_;
      copier_   = other.copier_;
      deleter_  = other.deleter_;
    }
  }

  // Move constructor.
  function(function&& other) noexcept
      : invoker_(other.invoker_), copier_(other.copier_), deleter_(other.deleter_) {
    if (other) {
      other.copier_(&other.storage_, &storage_);
      other.invoker_ = nullptr;
      other.copier_ = nullptr;
      other.deleter_ = nullptr;
    }
  }

  // Copy assignment.
  function& operator=(const function& other) {
    if (this != &other) {
      reset();
      if (other) {
        other.copier_(&other.storage_, &storage_);
        invoker_  = other.invoker_;
        copier_   = other.copier_;
        deleter_  = other.deleter_;
      }
    }
    return *this;
  }

  // Move assignment.
  function& operator=(function&& other) noexcept {
    if (this != &other) {
      reset();
      if (other) {
        other.copier_(&other.storage_, &storage_);
        invoker_  = other.invoker_;
        copier_   = other.copier_;
        deleter_  = other.deleter_;
        other.invoker_ = nullptr;
        other.copier_ = nullptr;
        other.deleter_ = nullptr;
      }
    }
    return *this;
  }

  // Destructor: calls the stored callable’s destructor if there is one.
  ~function() { reset(); }

  // Invocation operator. Calls the stored callable.
  R operator()(Args... args) const {
    assert(invoker_ != nullptr && "ftl::function is empty!");
    return invoker_(const_cast<void*>(reinterpret_cast<const void*>(&storage_)), args...);
  }

  // Boolean conversion: returns true if a callable is stored.
  explicit operator bool() const {
    return invoker_ != nullptr;
  }

  // Resets the function to an empty state (calls the destructor of the stored callable).
  void reset() {
    if (deleter_) {
      deleter_(&storage_);
    }
    invoker_ = nullptr;
    copier_ = nullptr;
    deleter_ = nullptr;
  }

 private:
  // Internal storage for the callable, properly aligned.
  typename std::aligned_storage<BufferSize>::type storage_;

  // Function pointers for invoking, copying, and destroying the stored callable.
  R (*invoker_)(void*, Args...){nullptr};
  void (*copier_)(const void*, void*){nullptr};
  void (*deleter_)(void*){nullptr};

  // Helper: Invokes the callable stored in the buffer.
  template <typename T>
  static R invoke(void* data, Args... args) {
    T* callable = reinterpret_cast<T*>(data);
    return (*callable)(args...);
  }

  // Helper: Copies the callable from src to dst.
  template <typename T>
  static void copy(const void* src, void* dst) {
    new (dst) T(*reinterpret_cast<const T*>(src));
  }

  // Helper: Calls the destructor of the callable stored in the buffer.
  template <typename T>
  static void destroy(void* data) {
    reinterpret_cast<T*>(data)->~T();
  }
};

}  // namespace ftl
