#pragma once

#include <stddef.h>
#include <stdint.h>
#include <atomic>
#include <new>      // For placement new, std::hardware_destructive_interference_size
#include <utility>  // For std::forward

namespace ftl {

// BumpAllocator provides a simple, linear ("bump") memory allocator.
// It allocates memory from a contiguous block, moving a pointer forward
// with each allocation.
//
// allocate() is lock-free (CAS on the bump pointer) and safe to call
// concurrently from threads and ISRs on a single core. It does not depend
// on any OS primitives, so it is usable before kernel start.
class BumpAllocator {
  static_assert(std::atomic<uint8_t*>::is_always_lock_free,
                "BumpAllocator requires lock-free atomic pointers");

 public:
  // Cache line size from hardware
  static constexpr size_t kCacheLineSize = std::hardware_destructive_interference_size;

  // Constructor.
  // @param base  Pointer to the beginning of the memory block.
  // @param size  Size of the memory block in bytes.
  // @param cache_align  If true, ensures all allocations start at cache line boundaries.
  BumpAllocator(uint8_t* base, size_t size, bool cache_align = false)
    : ptr_{base}, end_{base + size}, cache_align_{cache_align} {}

  // Allocate a block of memory.
  // @param size      The number of bytes to allocate.
  // @param alignment The alignment requirement (defaults to alignof(max_align_t)).
  // @return Pointer to the allocated memory if successful, or nullptr if out of memory.
  void* allocate(size_t size, size_t alignment = alignof(max_align_t)) {
    // If cache alignment is enabled, ensure allocations start at cache line boundaries
    if (cache_align_) {
      // Use the larger of the requested alignment or cache line size
      alignment = (alignment > kCacheLineSize) ? alignment : kCacheLineSize;
    }

    uint8_t* cur = ptr_.load(std::memory_order_relaxed);
    for (;;) {
      uintptr_t aligned = (reinterpret_cast<uintptr_t>(cur) + alignment - 1) & ~(alignment - 1);
      if (aligned + size > reinterpret_cast<uintptr_t>(end_)) {
        return nullptr;  // Out of memory.
      }
      uint8_t* next = reinterpret_cast<uint8_t*>(aligned + size);
      if (ptr_.compare_exchange_weak(cur, next,
              std::memory_order_acq_rel,
              std::memory_order_relaxed)) {
        return reinterpret_cast<void*>(aligned);
      }
      // compare_exchange_weak reloaded cur on failure; retry.
    }
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

  const uint8_t * head() const { return ptr_.load(std::memory_order_relaxed); }

 private:
  // Current allocation position.
  std::atomic<uint8_t*> ptr_;
  // One past the end of the memory block.
  uint8_t* end_ = nullptr;
  // Whether to prevent allocations from crossing cache line boundaries.
  bool cache_align_ = false;
};

}
