#pragma once

#include <memory>
#include <cstddef>
#include "ftl/bump_pool_allocator2.hpp"
#include "ftl/deleter.hpp"

namespace ftl {

/// Allocator that wraps BumpPoolAllocator2 and returns std::unique_ptr objects
/// with automatic memory management. The unique_ptr will automatically return
/// the memory to the pool when it goes out of scope.
template<typename T>
class UniqueAllocator : public PolymorphicDeleter<T> {
public:
    explicit UniqueAllocator(BumpPoolAllocator2& allocator) noexcept
        : allocator_(allocator) {}

    /// Override from PolymorphicDeleter - deallocates the object
    void operator()(T* ptr) override {
        if (ptr) {
            ptr->~T();
            allocator_.deallocate(ptr);
        }
    }

    /// Allocate and construct an object, returning it wrapped in a unique_ptr
    /// with a custom deleter that returns memory to the pool.
    /// @tparam Args Constructor argument types
    /// @param args Arguments forwarded to T's constructor
    /// @return unique_ptr that automatically returns memory to pool on destruction
    template<typename... Args>
    std::unique_ptr<T, DelegatingDeleter<T>> allocate_unique(Args&&... args) {
        // Allocate and construct the object
        T* ptr = allocator_.allocate<T>(std::forward<Args>(args)...);
        
        // Return unique_ptr with delegating deleter pointing to this allocator
        return std::unique_ptr<T, DelegatingDeleter<T>>(
            ptr, 
            DelegatingDeleter<T>(this)
        );
    }

private:
    BumpPoolAllocator2& allocator_;
};

}  // namespace ftl