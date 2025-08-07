#pragma once

#include <memory>
#include <cstddef>
#include <cassert>
#include "ftl/bump_pool.hpp"
#include "ftl/bump_allocator.hpp"

namespace ftl {

/// STL-compatible allocator that uses ftl::BumpPool for memory management.
/// Designed for node-based containers (std::map, std::list, std::set) that allocate one node at a time.
/// All instances with the same T type share a static pool.
template <typename T>
class BumpPoolAllocator {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;

    /// Rebind allocator to type U (required for STL containers)
    template <typename U>
    struct rebind {
        using other = BumpPoolAllocator<U>;
    };

    /// Initialize the shared pool for all allocators of this type
    /// Must be called before using any BumpPoolAllocator<T>
    static void initializePool(ftl::BumpAllocator& allocator, size_t initial_size = 1) {
        if (!getPool()) {
            // Allocate the pool object from the bump allocator
            void* pool_mem = allocator.allocate(sizeof(ftl::BumpPool<T>));
            if (pool_mem) {
                // Construct pool in-place, starts with initial_size nodes and grows as needed
                getPool() = new (pool_mem) ftl::BumpPool<T>(allocator, initial_size);
            }
        }
    }

    /// Default constructor
    BumpPoolAllocator() = default;

    /// Copy constructor
    BumpPoolAllocator(const BumpPoolAllocator&) = default;

    /// Rebinding copy constructor
    template <typename U>
    BumpPoolAllocator(const BumpPoolAllocator<U>&) {}

    /// Allocate memory for n objects
    /// Only supports n=1 for node-based containers
    T* allocate(size_type n) {
        // This allocator only supports single node allocations
        assert(n == 1 && "BumpPoolAllocator only supports single object allocation (for node-based containers)");
        
        ftl::BumpPool<T>* pool = getPool();
        assert(pool != nullptr && "BumpPoolAllocator pool not initialized");
        
        T* ptr = pool->acquire();
        assert(ptr != nullptr && "Failed to acquire from pool - pool may be exhausted");
        return ptr;
    }

    /// Deallocate memory for n objects
    void deallocate(T* p, size_type n) {
        if (p && n == 1) {
            ftl::BumpPool<T>* pool = getPool();
            if (pool) {
                pool->release(p);
            }
        }
    }

    /// Comparison operators (required for STL)
    bool operator==(const BumpPoolAllocator&) const { return true; }
    bool operator!=(const BumpPoolAllocator&) const { return false; }

private:
    /// Get the shared pool for all BumpPoolAllocators of type T
    static ftl::BumpPool<T>*& getPool() {
        static ftl::BumpPool<T>* shared_pool = nullptr;
        return shared_pool;
    }
};

}  // namespace ftl