#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>
#include "strategy.hpp"

namespace ftl::allocator {

namespace detail {
// Non-virtual malloc implementation to avoid code duplication
class MallocImpl {
private:
    std::size_t size_;
    std::size_t alignment_;
    
public:
    MallocImpl(std::size_t size, std::size_t alignment) noexcept
        : size_(size), alignment_(alignment) {}
    
    void* allocate() noexcept {
        // Round up size to multiple of alignment as required by aligned_alloc
        std::size_t aligned_size = (size_ + alignment_ - 1) & ~(alignment_ - 1);
        return std::aligned_alloc(alignment_, aligned_size);
    }
    
    void deallocate(void* ptr) noexcept {
        std::free(ptr);  // free works for both malloc and aligned_alloc
    }
};
} // namespace detail

// Malloc-based block allocation strategy
class MallocBlockStrategy : public IBlockStrategy {
private:
    detail::MallocImpl impl_;
    
public:
    MallocBlockStrategy(std::size_t size, std::size_t alignment = alignof(std::max_align_t)) noexcept
        : IBlockStrategy(size), impl_(size, alignment) {}
    
    void* allocate() noexcept override {
        return impl_.allocate();
    }
    
    void deallocate(void* ptr) noexcept override {
        impl_.deallocate(ptr);
    }
};

// Malloc-based object allocation strategy - delegates to MallocImpl
template <typename T>
class MallocObjStrategy : public IObjStrategy<T> {
private:
    detail::MallocImpl impl_;
    
public:
    MallocObjStrategy() noexcept
        : impl_(sizeof(T), alignof(T)) {}
    
    void* allocate() noexcept override {
        return impl_.allocate();
    }
    
    void deallocate(void* ptr) noexcept override {
        impl_.deallocate(ptr);
    }
};

} // namespace ftl::allocator
