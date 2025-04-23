#pragma once

#include <stddef.h>
#include <stdint.h>
#include <new>      // For placement new
#include <utility>  // For std::forward

namespace ftl {

// BumpAllocator provides a simple, linear ("bump") memory allocator.
// It allocates memory from a contiguous block, moving a pointer forward
// with each allocation. The entire block can be reset for reuse.
class BumpAllocator {
 public:
  // Constructor.
  // @param base  Pointer to the beginning of the memory block.
  // @param size  Size of the memory block in bytes.
  BumpAllocator(uint8_t* base, size_t size)
    : ptr_{base}, base_{base}, end_{base + size} {}

  // Allocate a block of memory.
  // @param size      The number of bytes to allocate.
  // @param alignment The alignment requirement (defaults to alignof(max_align_t)).
  // @return Pointer to the allocated memory if successful, or nullptr if out of memory.
  void* allocate(size_t size, size_t alignment = alignof(max_align_t)) {
    uintptr_t current = reinterpret_cast<uintptr_t>(ptr_);
    // Align the current pointer to the requested alignment.
    uintptr_t aligned = (current + alignment - 1) & ~(alignment - 1);
    // Check if there is enough memory remaining.
    if (aligned + size <= reinterpret_cast<uintptr_t>(end_)) {
      ptr_ = reinterpret_cast<uint8_t*>(aligned + size);
      return reinterpret_cast<void*>(aligned);
    }
    return nullptr;  // Out of memory.
  }

  // Template method to allocate memory for an object of type T and construct it.
  // Forwards constructor arguments to T's constructor.
  // @return Pointer to the newly constructed object, or nullptr on failure.
  template <typename T, typename... Args>
  T* allocate(Args&&... args) {
    void* raw_memory = allocate(sizeof(T));
    if (raw_memory == nullptr) {
      return nullptr;
    }
    return new (raw_memory) T(std::forward<Args>(args)...);
  }

  // Reset the allocator to reuse the entire memory block.
  // Resets the current pointer back to the base.
  void reset() {
    ptr_ = base_;
  }

 private:
  // Pointer to the current allocation position.
  uint8_t* ptr_ = nullptr;
  // Base of the memory block.
  uint8_t* base_ = nullptr;
  // End of the memory block.
  uint8_t* end_ = nullptr;
};

}
