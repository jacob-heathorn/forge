#pragma once

#include <memory>
#include <cstddef>
#include <cassert>
#include <cstdio>
#include "ftl/bump_pool.hpp"
#include "ftl/bump_allocator.hpp"

namespace ftl {

/// STL-compatible allocator that uses ftl::BumpPool for memory management.
/// IMPORTANT: This allocator only supports allocations of size 1.
/// Designed for node-based containers (std::map, std::list, std::set) that allocate one node at a time.
/// All instances with the same T type share a static pool.
template<typename T>
class BumpPoolAllocator {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::true_type;

    /// Initialize the shared pool for all allocators of this type
    /// Must be called before using any BumpPoolAllocator<T>
    static void initializePool(ftl::BumpAllocator& allocator) {
        assert(!pool_ && "Pool already initialized");
        // Use the allocator's template method to allocate and construct the pool
        pool_ = allocator.allocate<ftl::BumpPool<T>>(allocator);
        assert(pool_ && "Failed to allocate BumpPool");
    }

    BumpPoolAllocator() noexcept {
        assert(pool_ && "Pool not initialized! Call BumpPoolAllocator::initializePool() first");
    }

    template<typename U>
    BumpPoolAllocator(const BumpPoolAllocator<U>&) noexcept {
        assert(pool_ && "Pool not initialized! Call BumpPoolAllocator::initializePool() first");
    }

    T* allocate(size_type n) {
        assert(n == 1 && "BumpPoolAllocator only supports allocating one object at a time");
        assert(pool_ && "Pool not initialized! Call BumpPoolAllocator::initializePool() first");
        
        T* ptr = pool_->acquire();
        printf("BumpPoolAllocator: allocated %u bytes at %p\n", (unsigned)sizeof(T), ptr);
        return ptr;
    }

    void deallocate(T* p, size_type n) noexcept {
        assert(n == 1 && "BumpPoolAllocator only supports deallocating one object at a time");
        assert(pool_ && "Pool not initialized! Call BumpPoolAllocator::initializePool() first");
        
        printf("BumpPoolAllocator: deallocating %u bytes at %p\n", (unsigned)sizeof(T), p);
        pool_->release(p);
    }

private:
    inline static ftl::BumpPool<T>* pool_ = nullptr;
};

}  // namespace ftl