#pragma once

#include <cstddef>
#include <utility>
#include "ftl/object_allocator.hpp"
#include "ftl/bump_pool_allocation_strategy.hpp"
#include "ftl/bump_allocator.hpp"

namespace ftl {

/// Allocator that uses BumpPoolAllocationStrategy with ObjectAllocator
/// for memory management with automatic construction/destruction.
/// Templated at class level for a specific type T.
template<typename T>
class BumpPoolAllocator {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;

    /// Constructor that takes a BumpAllocator and optional initial pool size
    explicit BumpPoolAllocator(ftl::BumpAllocator& allocator, size_type initial_size = 1) 
        : strategy_(allocator, initial_size),
          object_allocator_(strategy_) {
    }

    /// Allocate and construct an object
    template<typename... Args>
    T* allocate(Args&&... args) {
        return object_allocator_.allocate(std::forward<Args>(args)...);
    }

    /// Deallocate an object (destruct and return it to the pool)
    void deallocate(T* p) noexcept {
        object_allocator_.deallocate(p);
    }

    /// Get total number of objects allocated in the pool
    size_type TotalSize() const {
        return strategy_.total_size();
    }

    /// Get number of free objects in the pool
    size_type FreeSize() const {
        return strategy_.free_size();
    }

    /// Get number of currently used objects in the pool
    size_type UsedSize() const {
        return strategy_.used_size();
    }

private:
    BumpPoolAllocationStrategy<T> strategy_;
    ObjectAllocator<T> object_allocator_;
};

}  // namespace ftl