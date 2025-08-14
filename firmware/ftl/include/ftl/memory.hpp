#pragma once

#include <utility>
#include <type_traits>
#include <cstddef>

namespace ftl {

/// @brief Abstract interface for object deletion
/// Used by ftl::unique_ptr to enable polymorphic deletion strategies
class Deleter {
public:
    virtual ~Deleter() = default;
    
    /// Delete the object pointed to by ptr
    /// @param ptr Pointer to object to delete (as void* for type erasure)
    virtual void operator()(void* ptr) = 0;
    
    // Prevent copying and moving to ensure deleter lifetime is managed properly
    Deleter(const Deleter&) = delete;
    Deleter& operator=(const Deleter&) = delete;
    Deleter(Deleter&&) = delete;
    Deleter& operator=(Deleter&&) = delete;
    
protected:
    Deleter() = default;
};

/// @brief Default deleter that uses delete operator
template<typename T>
class DefaultDeleter : public Deleter {
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
    Deleter* deleter_ = nullptr;
    
public:
    // Default constructor
    constexpr unique_ptr() noexcept = default;
    
    // Constructor with nullptr
    constexpr unique_ptr(std::nullptr_t) noexcept : ptr_(nullptr), deleter_(nullptr) {}
    
    // Constructor with pointer and deleter
    explicit unique_ptr(T* ptr, Deleter* deleter = nullptr) noexcept
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
    void reset(T* ptr, Deleter* deleter) noexcept {
        reset();
        ptr_ = ptr;
        deleter_ = deleter;
    }
    
    // Get raw pointer
    T* get() const noexcept { return ptr_; }
    
    // Get deleter
    Deleter* get_deleter() const noexcept { return deleter_; }
    
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