#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>
#include "strategy.hpp"

namespace ftl::allocator {
namespace detail {

struct MallocStrategyState {
    std::size_t block_size;
    std::size_t alignment;
};

inline void* malloc_allocate(void* self) noexcept {
    if (!self) return nullptr;
    auto* state = static_cast<MallocStrategyState*>(self);
    
    // Use aligned allocation if alignment is greater than default
    if (state->alignment > alignof(std::max_align_t)) {
        // Round up size to multiple of alignment for aligned_alloc
        std::size_t size = state->block_size;
        size = (size + state->alignment - 1) & ~(state->alignment - 1);
        return std::aligned_alloc(state->alignment, size);
    } else {
        return std::malloc(state->block_size);
    }
}

inline void malloc_deallocate(void* self, void* ptr) noexcept {
    (void)self;
    std::free(ptr);
}

template <typename T>
struct MallocObjState {
    std::size_t size;
    std::size_t alignment;
};

template <typename T>
inline void* malloc_obj_allocate(void* self) noexcept {
    if (!self) return nullptr;
    auto* state = static_cast<MallocObjState<T>*>(self);
    
    // For over-aligned allocations, use aligned_alloc
    if (state->alignment > alignof(std::max_align_t)) {
        // Size must be a multiple of alignment for aligned_alloc
        std::size_t size = state->size;
        size = (size + state->alignment - 1) & ~(state->alignment - 1);
        return std::aligned_alloc(state->alignment, size);
    } else {
        return std::malloc(state->size);
    }
}

template <typename T>
inline void malloc_obj_deallocate(void* self, void* ptr) noexcept {
    (void)self;
    std::free(ptr);
}

} // namespace detail

// Public factory class
struct MallocStrategy {
    // Create strategy for fixed-size blocks with optional alignment
    static BlockStrategy make(std::size_t block_size, std::size_t alignment = alignof(std::max_align_t)) noexcept {
        static thread_local detail::MallocStrategyState state;
        state.block_size = block_size;
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
        static thread_local detail::MallocObjState<T> state;
        state.size = sizeof(T);
        state.alignment = (alignment > alignof(T)) ? alignment : alignof(T);  // Use stronger of the two
        
        ObjStrategy<T> strat;
        strat.allocate = &detail::malloc_obj_allocate<T>;
        strat.deallocate = &detail::malloc_obj_deallocate<T>;
        strat.self = &state;
        return strat;
    }
};

} // namespace ftl::allocator