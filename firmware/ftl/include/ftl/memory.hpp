#pragma once

#include <utility>
#include <type_traits>
#include <cstddef>

namespace ftl {

/// @brief Abstract interface for object deletion with type erasure
///
/// Why we need type erasure (void* instead of templated):
/// 
/// In embedded systems with polymorphic factories, we need to return smart pointers
/// to base interfaces from different concrete allocators. For example:
///   - DogFactory returns unique_ptr<Animal> but allocates Dog objects
///   - CatFactory returns unique_ptr<Animal> but allocates Cat objects
///
/// Without type erasure:
///   - Each allocator would produce incompatible deleter types (IDeleter<Dog> vs IDeleter<Cat>)
///   - Factories couldn't implement the same interface
///   - Collections couldn't store objects from different factories
///
/// With type erasure (void*):
///   - All deleters share the same base interface (IDeleter)
///   - Different factories can return the same unique_ptr<Animal> type
///   - Collections can store mixed concrete types with proper cleanup
///
/// The void* cast is safe because:
///   - The deleter knows the actual type from construction
///   - The cast happens in controlled allocator code
///   - Type safety is maintained at the unique_ptr interface level
///
/// This design enables clean interfaces and dependency injection patterns
/// critical for testable, maintainable embedded systems.
class IDeleter {
public:
    virtual ~IDeleter() = default;
    
    /// Delete the object pointed to by ptr
    /// @param ptr Pointer to object to delete (as void* for type erasure)
    virtual void operator()(void* ptr) = 0;
    
    // Prevent copying and moving to ensure deleter lifetime is managed properly
    IDeleter(const IDeleter&) = delete;
    IDeleter& operator=(const IDeleter&) = delete;
    IDeleter(IDeleter&&) = delete;
    IDeleter& operator=(IDeleter&&) = delete;
    
protected:
    IDeleter() = default;
};

/// @brief Default deleter that uses delete operator
template<typename T>
class DefaultDeleter : public IDeleter {
public:
    void operator()(void* ptr) override {
        delete static_cast<T*>(ptr);
    }
};

/// @brief Smart pointer with unique ownership semantics for safety-critical embedded systems
/// Unlike std::unique_ptr, this can store a polymorphic deleter directly without wrapping
/// @tparam T Type of the managed object
template<typename T>
class unique_ptr {
private:
    T* ptr_ = nullptr;
    IDeleter* deleter_ = nullptr;
    
public:
    // Default constructor
    constexpr unique_ptr() noexcept = default;
    
    // Constructor with nullptr
    constexpr unique_ptr(std::nullptr_t) noexcept : ptr_(nullptr), deleter_(nullptr) {}
    
    // Constructor with pointer and deleter
    explicit unique_ptr(T* ptr, IDeleter* deleter = nullptr) noexcept
        : ptr_(ptr), deleter_(deleter) {}
    
    // Destructor - calls deleter if set
    ~unique_ptr() {
        reset();
    }
    
    // Delete copy constructor and assignment
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;
    
    // Move constructor
    unique_ptr(unique_ptr&& other) noexcept
        : ptr_(other.ptr_), deleter_(other.deleter_) {
        other.ptr_ = nullptr;
        other.deleter_ = nullptr;
    }
    
    // Move assignment
    unique_ptr& operator=(unique_ptr&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.ptr_;
            deleter_ = other.deleter_;
            other.ptr_ = nullptr;
            other.deleter_ = nullptr;
        }
        return *this;
    }
    
    // Assignment from nullptr
    unique_ptr& operator=(std::nullptr_t) noexcept {
        reset();
        return *this;
    }
    
    // Release ownership without deleting
    T* release() noexcept {
        T* tmp = ptr_;
        ptr_ = nullptr;
        deleter_ = nullptr;
        return tmp;
    }
    
    // Reset pointer, optionally to a new value
    void reset(T* ptr = nullptr) noexcept {
        if (ptr_ && deleter_) {
            deleter_->operator()(static_cast<void*>(ptr_));
        } else if (ptr_) {
            // No deleter set, use default delete
            delete ptr_;
        }
        ptr_ = ptr;
        if (!ptr) {
            deleter_ = nullptr;
        }
    }
    
    // Reset with new pointer and deleter
    void reset(T* ptr, IDeleter* deleter) noexcept {
        reset();
        ptr_ = ptr;
        deleter_ = deleter;
    }
    
    // Get raw pointer
    T* get() const noexcept { return ptr_; }
    
    // Get deleter
    IDeleter* get_deleter() const noexcept { return deleter_; }
    
    // Dereference operators
    T& operator*() const noexcept { return *ptr_; }
    T* operator->() const noexcept { return ptr_; }
    
    // Boolean conversion
    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    
    // Array subscript (for array types)
    template<typename U = T>
    typename std::enable_if_t<std::is_array_v<U>, typename std::remove_extent_t<U>&>
    operator[](std::size_t i) const noexcept {
        return get()[i];
    }
};

// Comparison operators
template<typename T>
bool operator==(const unique_ptr<T>& lhs, const unique_ptr<T>& rhs) noexcept {
    return lhs.get() == rhs.get();
}

template<typename T>
bool operator!=(const unique_ptr<T>& lhs, const unique_ptr<T>& rhs) noexcept {
    return !(lhs == rhs);
}

template<typename T>
bool operator==(const unique_ptr<T>& lhs, std::nullptr_t) noexcept {
    return !lhs;
}

template<typename T>
bool operator==(std::nullptr_t, const unique_ptr<T>& rhs) noexcept {
    return !rhs;
}

template<typename T>
bool operator!=(const unique_ptr<T>& lhs, std::nullptr_t) noexcept {
    return bool(lhs);
}

template<typename T>
bool operator!=(std::nullptr_t, const unique_ptr<T>& rhs) noexcept {
    return bool(rhs);
}


}  // namespace ftl