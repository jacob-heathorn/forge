#pragma once

#include <cstddef>
#include <utility>
#include <new>
#include "strategy.hpp"

namespace ftl::allocator {

/// Allocator that uses an IObjStrategy for memory management
/// and handles object construction/destruction
/// @tparam T Type of objects to allocate
template<typename T>
class ObjAllocator {
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

private:
    IObjStrategy<T>& strategy_;
};

}  // namespace ftl::allocator