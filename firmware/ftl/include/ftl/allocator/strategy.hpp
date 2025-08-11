#pragma once

#include <cstddef>
#include <cstdint>

namespace ftl::allocator {

// ============================================================================
// Interface allocation strategies, buffers (block) OR objects
// ============================================================================
struct BlockStrategy {
  void* (*allocate)(void* self) noexcept;  // returns nullptr on failure
  void  (*deallocate)(void* self, void* ptr) noexcept;
  void* self{nullptr};                      // points to per-slot state
};

// For exactly one T. Alignment is baked into the strategy.
template <typename T>
struct ObjStrategy {
  void* (*allocate)(void* self) noexcept;   // returns storage for one T
  void  (*deallocate)(void* self, void* p) noexcept;
  void* self{nullptr};
};


}
