#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>
#include "strategy.hpp"

namespace ftl::allocator {
namespace detail {

struct MallocStrategyState {
    std::size_t block_size;
};

inline void* malloc_allocate(void* self, std::size_t align) noexcept {
    if (!self) return nullptr;
    auto* state = static_cast<MallocStrategyState*>(self);
    
    // Use aligned allocation if alignment is greater than default
    if (align > alignof(std::max_align_t)) {
        // Round up size to multiple of alignment for aligned_alloc
        std::size_t size = state->block_size;
        size = (size + align - 1) & ~(align - 1);
        return std::aligned_alloc(align, size);
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
    static constexpr std::size_t size = sizeof(T);
};

template <typename T>
inline void* malloc_obj_allocate(void* self, std::size_t align) noexcept {
    (void)self;
    
    // Use the requested alignment or the type's natural alignment, whichever is stronger
    std::size_t effective_align = (align > alignof(T)) ? align : alignof(T);
    
    // For over-aligned allocations, use aligned_alloc
    if (effective_align > alignof(std::max_align_t)) {
        // Size must be a multiple of alignment for aligned_alloc
        std::size_t size = sizeof(T);
        size = (size + effective_align - 1) & ~(effective_align - 1);
        return std::aligned_alloc(effective_align, size);
    } else {
        return std::malloc(sizeof(T));
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
    static BlockStrategy make(std::size_t block_size) noexcept {
        static thread_local detail::MallocStrategyState state;
        state.block_size = block_size;
        
        BlockStrategy strat;
        strat.allocate = &detail::malloc_allocate;
        strat.deallocate = &detail::malloc_deallocate;
        strat.self = &state;
        return strat;
    }
    
    template <typename T>
    static ObjStrategy<T> make_for() noexcept {
        static detail::MallocObjState<T> state;
        
        ObjStrategy<T> strat;
        strat.allocate = &detail::malloc_obj_allocate<T>;
        strat.deallocate = &detail::malloc_obj_deallocate<T>;
        strat.self = &state;
        return strat;
    }
};

} // namespace ftl::allocator