#pragma once

#include <cstddef>
#include <cassert>
#include <cstdio>
#include "ftl/bump_pool.hpp"
#include "ftl/bump_allocator.hpp"

namespace ftl {

/// Allocator that uses ftl::BumpPool for memory management.
/// Non-templated class with templated methods for allocation.
/// Uses lazy initialization of per-type pools.
class BumpPoolAllocator2 {
public:
    using size_type = std::size_t;

    explicit BumpPoolAllocator2(ftl::BumpAllocator& allocator) noexcept 
        : allocator_(allocator) {
    }

    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        auto& p = pool<T>();
        T* ptr = p.acquire(std::forward<Args>(args)...);
        // printf("BumpPoolAllocator2: allocated %u bytes at %p\n", (unsigned)sizeof(T), ptr);
        return ptr;
    }

    template<typename T>
    void deallocate(T* p) noexcept {
        // printf("BumpPoolAllocator2: deallocating %u bytes at %p\n", (unsigned)sizeof(T), p);
        pool<T>().release(p);
    }

    /// Get total number of objects allocated in the pool for type T
    template<typename T>
    size_type TotalSize() {
        return pool<T>().TotalSize();
    }

    /// Get number of free objects in the pool for type T
    template<typename T>
    size_type FreeSize() {
        return pool<T>().FreeSize();
    }

    /// Get number of currently used objects in the pool for type T
    template<typename T>
    size_type UsedSize() {
        return pool<T>().UsedSize();
    }

private:
    /// Get or lazily initialize the pool for type T
    template<typename T>
    ftl::BumpPool<T>& pool() {
        static ftl::BumpPool<T>* pool_ptr = nullptr;
        
        if (!pool_ptr) {
            pool_ptr = allocator_.allocate<ftl::BumpPool<T>>(allocator_);
            assert(pool_ptr && "Failed to allocate BumpPool");
        }
        
        return *pool_ptr;
    }

    ftl::BumpAllocator& allocator_;
};

}  // namespace ftl