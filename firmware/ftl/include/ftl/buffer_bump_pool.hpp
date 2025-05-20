#pragma once

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>               // for placement new
#include "etl/array.h"

#include "ftl/bump_allocator.hpp"
#include "ftl/mutex.hpp"

//-------------------------------------------------------------------------------------------------
// BufferBumpPool: fixed‐capacity buffer pool with 64‐byte alignment.
// 
// — Max buffer size: 2048 bytes
// — Size classes: multiples of 64 bytes (1×64 … 32×64)
// — Per‐size lock‐free free‐lists in a fixed etl::array.
// — Only bump‐allocator growth is mutex‐protected; all free‐list ops are pure atomics.
//-------------------------------------------------------------------------------------------------
class BufferBumpPool {
public:
  explicit BufferBumpPool(ftl::BumpAllocator& alloc)
    : allocator_(alloc)
  {
    // initialize all free‐list heads to nullptr
    for (auto &h : heads_) {
      h.store(nullptr, std::memory_order_relaxed);
    }
  }

  ~BufferBumpPool() = default;

  BufferBumpPool(const BufferBumpPool&) = delete;
  BufferBumpPool& operator=(const BufferBumpPool&) = delete;
  BufferBumpPool(BufferBumpPool&&) = delete;
  BufferBumpPool& operator=(BufferBumpPool&&) = delete;

  // Acquire a buffer of up to 'size' bytes (rounded up to next 64‐byte slot).
  // Returns a 64‐byte aligned pointer.
  uint8_t* acquire(std::size_t size) {
    const std::size_t slot = slotForSize(size);
    // try lock‐free pop
    Node* node = popNode_(slot);
    if (!node) {
      // free‐list empty → bump‐allocate new Node+payload (with padding for alignment)
      node = allocateNode_(slot);
    }
    // payload begins immediately after Node header
    return reinterpret_cast<uint8_t*>(node + 1);
  }

  // Release a buffer back into its slot's free‐list.
  // Must pass the same 'size' used to acquire.
  void release(uint8_t* buffer, std::size_t size) noexcept {
    if (!buffer) return;
    // recover the Node header
    Node* node = reinterpret_cast<Node*>(buffer) - 1;
    const std::size_t slot = slotForSize(size);
    pushNode_(slot, node);
  }

private:
  static constexpr std::size_t ALIGN       = 64;
  static constexpr std::size_t MAX_SIZE    = 2048;
  static constexpr std::size_t NUM_SLOTS   = MAX_SIZE / ALIGN;

  // Each node holds one free buffer of size (slot+1)*ALIGN
  struct Node {
    Node* next;
  };

  // Round up 'size' to next multiple of ALIGN and map to slot index [0..31]
  static std::size_t slotForSize(std::size_t size) {
    std::size_t buckets = (size + ALIGN - 1) / ALIGN;
    if (buckets == 0) buckets = 1;
    if (buckets > NUM_SLOTS) buckets = NUM_SLOTS;
    return buckets - 1;
  }

  // Pop a node from the atomic free‐list for slot 'i'
  Node* popNode_(std::size_t i) noexcept {
    auto &head = heads_[i];
    Node* curr = head.load(std::memory_order_acquire);
    while (curr) {
      Node* next = curr->next;
      if (head.compare_exchange_weak(
            curr, next,
            std::memory_order_acquire,
            std::memory_order_relaxed))
      {
        return curr;
      }
      // curr updated to new head; retry
    }
    return nullptr;
  }

  // Push a node onto the atomic free‐list for slot 'i'
  void pushNode_(std::size_t i, Node* node) noexcept {
    auto &head = heads_[i];
    Node* curr = head.load(std::memory_order_relaxed);
    do {
      node->next = curr;
    } while (!head.compare_exchange_weak(
               curr, node,
               std::memory_order_release,
               std::memory_order_relaxed));
  }

  //-------------------------------------------------------------------------------------------------
  // Allocate a fresh Node + payload for slot 'i', ensuring the
  // *payload* is 64-byte aligned. We carve out enough extra bytes
  // so that we can slide the payload up to a ALIGN boundary
  // and stick our Node header immediately before it.
  //-------------------------------------------------------------------------------------------------
  Node* allocateNode_(std::size_t i) {
    const std::size_t bufBytes   = (i + 1) * ALIGN;
    // extra ALIGN-1 guarantees we can align the payload
    const std::size_t totalBytes = sizeof(Node) + bufBytes + (ALIGN - 1);

    void* rawPtr;
    {
      ftl::LockGuard<ftl::Mutex> lock(allocatorMutex_);
      rawPtr = allocator_.allocate(totalBytes);
    }

    uintptr_t base       = reinterpret_cast<uintptr_t>(rawPtr);
    // payloadAddr = next multiple of ALIGN after (base + sizeof(Node))
    uintptr_t payloadAddr = (base + sizeof(Node) + (ALIGN - 1)) & ~(ALIGN - 1);
    // put Node header immediately before the payload
    uintptr_t nodeAddr    = payloadAddr - sizeof(Node);

    auto* node = reinterpret_cast<Node*>(nodeAddr);
    node->next = nullptr;
    return node;
  }

  ftl::BumpAllocator&                          allocator_;
  ftl::Mutex                                   allocatorMutex_;  
  etl::array<std::atomic<Node*>, NUM_SLOTS>    heads_;
};
