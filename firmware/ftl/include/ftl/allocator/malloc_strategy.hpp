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

inline void* malloc_allocate(void* self) noexcept {
    if (!self) return nullptr;
    auto* state = static_cast<MallocStrategyState*>(self);
    return std::malloc(state->block_size);
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
inline void* malloc_obj_allocate(void* self) noexcept {
    (void)self;
    
    // For over-aligned types, use aligned allocation
    if constexpr (alignof(T) > alignof(std::max_align_t)) {
        // Use aligned_alloc for over-aligned types
        // Size must be a multiple of alignment for aligned_alloc
        std::size_t size = sizeof(T);
        std::size_t alignment = alignof(T);
        // Round up size to multiple of alignment
        size = (size + alignment - 1) & ~(alignment - 1);
        return std::aligned_alloc(alignment, size);
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