#pragma once

#include <stddef.h>
#include <stdint.h>
#include <atomic>
#include <new>
#include <utility>

namespace ftl {

// Linear bump allocator over a caller-provided memory block.
// allocate() is lock-free (CAS on the bump pointer) and safe to call
// concurrently from threads and ISRs on a single core. Uses no OS
// primitives, so it is usable before kernel start.
//
// Thread safety hinges on ptr_ being monotonic — it only ever advances.
// A stale observation is always a lower bound (CAS catches it; no ABA),
// and handed-out slots are never recycled. This is why there is no
// reset(): resetting would break monotonicity and let already-allocated
// slots be handed out again.
class BumpAllocator {
  static_assert(std::atomic<uint8_t*>::is_always_lock_free,
                "BumpAllocator requires lock-free atomic pointers");

 public:
  static constexpr size_t kCacheLineSize = std::hardware_destructive_interference_size;

  // If cache_align is true, every allocation starts on a cache line boundary.
  BumpAllocator(uint8_t* base, size_t size, bool cache_align = false)
    : ptr_{base}, end_{base + size}, cache_align_{cache_align} {}

  void* allocate(size_t size, size_t alignment = alignof(max_align_t)) {
    // end_ and cache_align_ are immutable after construction, so reading
    // them while other threads allocate is data-race-free.
    if (cache_align_) {
      alignment = (alignment > kCacheLineSize) ? alignment : kCacheLineSize;
    }

    // Relaxed load: a stale cur just loses the CAS below and we retry.
    // The CAS itself provides the synchronizing memory order.
    uint8_t* cur = ptr_.load(std::memory_order_relaxed);
    for (;;) {
      uintptr_t aligned = (reinterpret_cast<uintptr_t>(cur) + alignment - 1) & ~(alignment - 1);
      if (aligned + size > reinterpret_cast<uintptr_t>(end_)) {
        return nullptr;
      }
      uint8_t* next = reinterpret_cast<uint8_t*>(aligned + size);
      // CAS serializes bumpers: at most one thread advances ptr_ from cur
      // to next, so the slot [aligned, next) is exclusively owned by the
      // winner. acq_rel ensures we observe every prior winner's bump and
      // publish ours to subsequent winners.
      if (ptr_.compare_exchange_weak(cur, next,
              std::memory_order_acq_rel,
              std::memory_order_relaxed)) {
        return reinterpret_cast<void*>(aligned);
      }
      // CAS failed (another thread bumped, or spurious weak failure);
      // cur was reloaded with the current value, retry.
    }
  }

  template <typename T, typename... Args>
  T* allocate(Args&&... args) {
    // The raw slot returned is exclusively owned by this thread (CAS
    // winner), so placement-new into it cannot race.
    void* raw_memory = allocate(sizeof(T));
    if (raw_memory == nullptr) {
      return nullptr;
    }
    return new (raw_memory) T(std::forward<Args>(args)...);
  }

  // Relaxed snapshot; value may already be stale when the caller uses it.
  const uint8_t * head() const { return ptr_.load(std::memory_order_relaxed); }

 private:
  // All mutation goes through atomic ops on ptr_.
  std::atomic<uint8_t*> ptr_;
  uint8_t* end_ = nullptr;    // one-past-the-end, immutable after ctor
  bool cache_align_ = false;  // immutable after ctor
};

}
