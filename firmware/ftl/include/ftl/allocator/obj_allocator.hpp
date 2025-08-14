#pragma once

#include <cstddef>
#include <utility>
#include <new>
#include "strategy.hpp"
#include "ftl/memory.hpp"

namespace ftl::allocator {

/// Allocator that uses an IObjStrategy for memory management
/// and handles object construction/destruction
/// @tparam T Type of objects to allocate
template<typename T>
class ObjAllocator : public ftl::IDeleter {
public:
    /// Constructor that takes a reference to an IObjStrategy
    /// @param strategy Object allocation strategy to use (must outlive this allocator)
    explicit ObjAllocator(IObjStrategy<T>& strategy) 
        : strategy_(strategy) {
    }

    /// Allocate memory and construct an object
    /// @tparam Args Constructor argument types
    /// @param args Arguments forwarded to T's constructor
    /// @return Pointer to newly constructed object, or nullptr if allocation fails
    template<typename... Args>
    T* allocate(Args&&... args) {
        void* raw = strategy_.allocate();
        if (raw) {
            // Construct object using placement new
            return ::new(raw) T(std::forward<Args>(args)...);
        }
        return nullptr;
    }

    /// Destruct an object and deallocate its memory
    /// @param p Pointer to object to destroy
    void deallocate(T* p) noexcept {
        if (p) {
            p->~T();  // Destruct the object
            strategy_.deallocate(p);  // Return memory to strategy
        }
    }

    /// Override polymorphic deleter call: destruct and deallocate object
    /// @param ptr Pointer to the object to release (as void*)
    void operator()(void* ptr) override {
        deallocate(static_cast<T*>(ptr));
    }

    /// Create an object and wrap it in a unique_ptr
    /// @tparam B Base type for the unique_ptr (T must be derived from B, defaults to T)
    /// @tparam Args Constructor argument types
    /// @param args Arguments forwarded to T's constructor
    /// @return ftl::unique_ptr<B> that deallocates on destruction
    template<typename B = T, typename... Args>
    ftl::unique_ptr<B> make_unique(Args&&... args) {
        static_assert(std::is_base_of_v<B, T> || std::is_same_v<B, T>, 
                      "T must be derived from B or the same as B");
        T* obj = allocate(std::forward<Args>(args)...);
        return ftl::unique_ptr<B>(obj, this);
    }

private:
    IObjStrategy<T>& strategy_;
};

}  // namespace ftl::allocator