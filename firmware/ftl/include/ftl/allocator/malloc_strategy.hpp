#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>
#include "strategy.hpp"

namespace ftl::allocator {
namespace detail {

struct MallocState {
    std::size_t size;
    std::size_t alignment;
};

inline void* malloc_allocate(void* self) noexcept {
    if (!self) return nullptr;
    auto* state = static_cast<MallocState*>(self);
    
    // Always use aligned_alloc for consistency
    // Round up size to multiple of alignment as required by aligned_alloc
    std::size_t aligned_size = (state->size + state->alignment - 1) & ~(state->alignment - 1);
    return std::aligned_alloc(state->alignment, aligned_size);
}

inline void malloc_deallocate(void* self, void* ptr) noexcept {
    (void)self;
    std::free(ptr);  // free works for both malloc and aligned_alloc
}

} // namespace detail

// Public factory class
struct MallocStrategy {
    // Create strategy for fixed-size blocks with optional alignment
    static BlockStrategy make(std::size_t block_size, std::size_t alignment = alignof(std::max_align_t)) noexcept {
        static thread_local detail::MallocState state;
        state.size = block_size;
        state.alignment = alignment;
        
        BlockStrategy strat;
        strat.allocate = &detail::malloc_allocate;
        strat.deallocate = &detail::malloc_deallocate;
        strat.self = &state;
        return strat;
    }
    
    // Create strategy for objects of type T with optional custom alignment
    // Default alignment is alignof(T), but can specify stronger alignment (e.g., cache line)
    template <typename T>
    static ObjStrategy<T> make_for(std::size_t alignment = alignof(T)) noexcept {
        static thread_local detail::MallocState state;
        state.size = sizeof(T);
        state.alignment = (alignment > alignof(T)) ? alignment : alignof(T);  // Use stronger of the two
        
        ObjStrategy<T> strat;
        strat.allocate = &detail::malloc_allocate;
        strat.deallocate = &detail::malloc_deallocate;
        strat.self = &state;
        return strat;
    }
};

} // namespace ftl::allocator
