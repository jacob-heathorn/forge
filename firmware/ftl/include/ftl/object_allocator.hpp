#pragma once

#include <cstddef>
#include <utility>
#include <new>
#include "ftl/allocation_strategy.hpp"

namespace ftl {

/// Allocator that uses an AllocationStrategy for memory management
/// and handles object construction/destruction
/// @tparam T Type of objects to allocate
template<typename T>
class ObjectAllocator {
public:
    /// Constructor that takes a reference to an AllocationStrategy
    /// @param strategy Allocation strategy to use (must outlive this allocator)
    explicit ObjectAllocator(AllocationStrategy<T>& strategy) 
        : strategy_(strategy) {
    }

    /// Allocate memory and construct an object
    /// @tparam Args Constructor argument types
    /// @param args Arguments forwarded to T's constructor
    /// @return Pointer to newly constructed object, or nullptr if allocation fails
    template<typename... Args>
    T* allocate(Args&&... args) {
        T* ptr = strategy_.allocate();
        if (ptr) {
            // Construct object using placement new
            return ::new(ptr) T(std::forward<Args>(args)...);
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
    AllocationStrategy<T>& strategy_;
};

}  // namespace ftl
