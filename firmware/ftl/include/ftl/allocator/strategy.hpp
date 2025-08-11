#pragma once

#include <cstddef>
#include <cstdint>

namespace ftl::allocator {

// ============================================================================
// Interface allocation strategies, buffers (block) OR objects
// ============================================================================
struct BlockStrategy {
    // For fixed-size buffers/blocks (size baked into state; no size arg)
    void* (*allocate)(void* self) noexcept;
    void  (*deallocate)(void* self, void* ptr) noexcept;
    void* self{nullptr};
};

// Typed strategy for objects: always allocates exactly one T
template <typename T>
struct ObjStrategy {
    void* (*allocate)(void* self) noexcept;   // returns storage for one T
    void  (*deallocate)(void* self, void* p) noexcept;
    void* self{nullptr};
};



}
