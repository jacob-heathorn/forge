#pragma once

#include <cstddef>
#include <cstdint>
#include <new>               // for placement new
#include "etl/array.h"

#include "ftl/bump_allocator.hpp"
#include "ftl/mutex.hpp"

//-------------------------------------------------------------------------------------------------
// BufferBumpPool
//
// A thread-safe buffer allocator that hands out fixed-size, 64-byte-aligned
// blocks from a bump allocator.  Buffers are segregated into size classes
// (64-byte multiples up to 2048 bytes) and recycled via per-class free lists.
// All operations (acquire/release) are protected by a single mutex to ensure
// correctness under multithreading.
//-------------------------------------------------------------------------------------------------
class BufferBumpPool {
public:
  explicit BufferBumpPool(ftl::BumpAllocator& alloc)
    : allocator_(alloc)
  {
    for (auto &h : heads_) {
      h = nullptr;
    }
  }

  ~BufferBumpPool() = default;

  BufferBumpPool(const BufferBumpPool&) = delete;
  BufferBumpPool& operator=(const BufferBumpPool&) = delete;
  BufferBumpPool(BufferBumpPool&&) = delete;
  BufferBumpPool& operator=(BufferBumpPool&&) = delete;

  // Acquire a buffer of at least 'size' bytes (rounded up to the next
  // 64-byte multiple).  Returns a pointer aligned to 64 bytes.
  uint8_t* acquire(std::size_t size) {
    const std::size_t slot = slotForSize(size);

    // attempt to get from free list
    Node* node = popNode_(slot);
    if (!node) {
      // allocate a new node+payload
      node = allocateNode_(slot);
    }
    // payload immediately follows Node header
    return reinterpret_cast<uint8_t*>(node + 1);
  }

  // Return a buffer previously acquired with the same 'size'.
  // Thread-safe: pushes the node back onto its slot’s free list.
  void release(uint8_t* buffer, std::size_t size) noexcept {
    if (!buffer) return;
    Node* node = reinterpret_cast<Node*>(buffer) - 1;
    const std::size_t slot = slotForSize(size);
    pushNode_(slot, node);
  }

private:
  static constexpr std::size_t ALIGN     = 64;
  static constexpr std::size_t MAX_SIZE  = 2048;
  static constexpr std::size_t NUM_SLOTS = MAX_SIZE / ALIGN;

  struct Node {
    Node* next;
  };

  // Map an arbitrary size to a slot index [0 .. NUM_SLOTS-1]
  static std::size_t slotForSize(std::size_t size) {
    std::size_t buckets = (size + ALIGN - 1) / ALIGN;
    if (buckets == 0) buckets = 1;
    if (buckets > NUM_SLOTS) buckets = NUM_SLOTS;
    return buckets - 1;
  }

  // Pop one node from slot 'i', or nullptr if empty
  Node* popNode_(std::size_t i) {
    ftl::LockGuard<ftl::Mutex> lock(mutex_);
    Node* node = heads_[i];
    if (node) {
      heads_[i] = node->next;
    }
    return node;
  }

  // Push one node onto slot 'i'
  void pushNode_(std::size_t i, Node* node) noexcept {
    ftl::LockGuard<ftl::Mutex> lock(mutex_);
    node->next   = heads_[i];
    heads_[i]    = node;
  }

  // Allocate a new Node + payload for slot 'i', ensuring the payload
  // is 64-byte aligned.
  Node* allocateNode_(std::size_t i) {
    const std::size_t bufBytes   = (i + 1) * ALIGN;
    const std::size_t totalBytes = sizeof(Node) + bufBytes + (ALIGN - 1);

    void* rawPtr;
    {
      ftl::LockGuard<ftl::Mutex> lock(mutex_);
      rawPtr = allocator_.allocate(totalBytes);
    }

    uintptr_t base        = reinterpret_cast<uintptr_t>(rawPtr);
    uintptr_t payloadAddr = (base + sizeof(Node) + (ALIGN - 1)) & ~(ALIGN - 1);
    uintptr_t nodeAddr    = payloadAddr - sizeof(Node);

    auto* node = reinterpret_cast<Node*>(nodeAddr);
    node->next = nullptr;
    return node;
  }

  ftl::BumpAllocator&              allocator_;
  ftl::Mutex                       mutex_;
  etl::array<Node*, NUM_SLOTS>     heads_;
};
