#pragma once

#include <cstddef>
#include <utility>
#include "ftl/bump_pool.hpp"
#include "ftl/bump_allocator.hpp"

namespace ftl {

/// Allocator that uses ftl::BumpPool for memory management.
/// Templated at class level for a specific type T.
/// Provides allocation and deallocation for objects of type T.
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
        : pool_(allocator, initial_size) {
    }

    /// Allocate and construct an object
    template<typename... Args>
    T* allocate(Args&&... args) {
        return pool_.acquire(std::forward<Args>(args)...);
    }

    /// Deallocate an object (return it to the pool)
    void deallocate(T* p) noexcept {
        if (p) {
            pool_.release(p);
        }
    }

    /// Get total number of objects allocated in the pool
    size_type TotalSize() const {
        return pool_.TotalSize();
    }

    /// Get number of free objects in the pool
    size_type FreeSize() const {
        return pool_.FreeSize();
    }

    /// Get number of currently used objects in the pool
    size_type UsedSize() const {
        return pool_.UsedSize();
    }

private:
    ftl::BumpPool<T> pool_;
};

}  // namespace ftl