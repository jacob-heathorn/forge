#pragma once

#include <cstddef>
#include <cstdint>

namespace ftl::allocator {

// ============================================================================
// Interface allocation strategies, buffers (block) OR objects
// ============================================================================
struct BlockStrategy {
  void* (*allocate)(void* self, std::size_t align) noexcept;  // nullptr if cannot meet align
  void  (*deallocate)(void* self, void* ptr) noexcept;
  void* self{nullptr};                                         // points to per-slot state
};

// For exactly one T. Caller may request stronger alignment than alignof(T).
template <typename T>
struct ObjStrategy {
  void* (*allocate)(void* self, std::size_t align) noexcept;   // align >= alignof(T) recommended
  void  (*deallocate)(void* self, void* p) noexcept;
  void* self{nullptr};
};

}
