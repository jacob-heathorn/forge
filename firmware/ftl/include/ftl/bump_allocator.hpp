#pragma once

#include <stddef.h>
#include <stdint.h>
#include <new>      // For placement new, std::hardware_destructive_interference_size
#include <utility>  // For std::forward

namespace ftl {

// BumpAllocator provides a simple, linear ("bump") memory allocator.
// It allocates memory from a contiguous block, moving a pointer forward
// with each allocation. The entire block can be reset for reuse.
class BumpAllocator {
 public:
  // Cache line size from hardware
  static constexpr size_t kCacheLineSize = std::hardware_destructive_interference_size;
  
  // Constructor.
  // @param base  Pointer to the beginning of the memory block.
  // @param size  Size of the memory block in bytes.
  // @param cache_align  If true, ensures allocations smaller than a cache line don't cross cache line boundaries.
  BumpAllocator(uint8_t* base, size_t size, bool cache_align = false)
    : ptr_{base}, base_{base}, end_{base + size}, cache_align_{cache_align} {}

  // Allocate a block of memory.
  // @param size      The number of bytes to allocate.
  // @param alignment The alignment requirement (defaults to alignof(max_align_t)).
  // @return Pointer to the allocated memory if successful, or nullptr if out of memory.
  void* allocate(size_t size, size_t alignment = alignof(max_align_t)) {
    uintptr_t current = reinterpret_cast<uintptr_t>(ptr_);
    // Align the current pointer to the requested alignment.
    uintptr_t aligned = (current + alignment - 1) & ~(alignment - 1);
    
    // If cache alignment is enabled and allocation fits within a cache line,
    // ensure it doesn't cross cache line boundaries
    if (cache_align_ && size <= kCacheLineSize) {
      uintptr_t cache_line_start = aligned & ~(kCacheLineSize - 1);
      uintptr_t cache_line_end = cache_line_start + kCacheLineSize;
      
      // If the allocation would cross a cache line boundary, bump to next cache line
      if (aligned + size > cache_line_end) {
        aligned = cache_line_end;
        // Re-apply the alignment requirement in case it's larger than cache line
        if (alignment > kCacheLineSize) {
          aligned = (aligned + alignment - 1) & ~(alignment - 1);
        }
      }
    }
    
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

  const uint8_t * head() const { return ptr_; }

 private:
  // Pointer to the current allocation position.
  uint8_t* ptr_ = nullptr;
  // Base of the memory block.
  uint8_t* base_ = nullptr;
  // One past the end of the memory block.
  uint8_t* end_ = nullptr;
  // Whether to prevent allocations from crossing cache line boundaries.
  bool cache_align_ = false;
};

}
