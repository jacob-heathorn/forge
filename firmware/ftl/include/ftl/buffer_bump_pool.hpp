#pragma once

#include <cstddef>
#include <cstdint>
#include <new>               // for placement new
#include <cassert>
#include <cstddef>           // for offsetof
#include "etl/array.h"

#include "ftl/buffer.hpp"
#include "ftl/bump_allocator.hpp"
#include "ftl/mutex.hpp"

namespace ftl {

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

  // Acquire a Buffer of at least 'size' bytes (rounded up to the next
  // 64-byte multiple).  Returns a pointer to an ftl::Buffer which holds
  // a 64-byte-aligned block of at least that size.
  Buffer* acquire(std::size_t size) {
    assert(size <= MAX_SIZE && "Requested size exceeds maximum buffer size");
    const std::size_t slot = slotForSize(size);

    // attempt to get from free list
    Node* node = popNode_(slot);
    if (!node) {
      // allocate a new node+payload
      node = allocateNode_(slot);
    }

    // Construct the Buffer handle in-place
    std::size_t bufBytes = (slot + 1) * ALIGN;
    new (&node->buf) Buffer{ reinterpret_cast<uint8_t*>(node + 1), bufBytes };
    return &node->buf;
  }

  // Return a Buffer previously acquired.  Thread-safe: pushes the node
  // back onto its slot’s free list.
  void release(Buffer* buffer) noexcept {
    if (!buffer) return;

    // Recover the Node* from the Buffer* via offsetof
    auto nodePtr = reinterpret_cast<uint8_t*>(buffer) - offsetof(Node, buf);
    Node* node = reinterpret_cast<Node*>(nodePtr);

    const std::size_t slot = slotForSize(buffer->size());
    pushNode_(slot, node);
  }

private:
  static constexpr std::size_t ALIGN     = 64;
  static constexpr std::size_t MAX_SIZE  = 2048;
  static constexpr std::size_t NUM_SLOTS = MAX_SIZE / ALIGN;

  struct Node {
    Node* next;
    Buffer buf;    // in-place handle to the payload
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
    const std::size_t bufBytes = (i + 1) * ALIGN;
    size_t pad = 0;
    void*  rawPtr = nullptr;

    {
      // Protect head() read, pad-calc, and allocate() in one critical section
      ftl::LockGuard<ftl::Mutex> lock(mutex_);

      // 1) snapshot current head and compute how many bytes of padding are needed
      uintptr_t base      = reinterpret_cast<uintptr_t>(allocator_.head()) + sizeof(Node);
      pad = (ALIGN - (base % ALIGN)) % ALIGN;

      // 2) ask for exactly metadata + pad + payload
      size_t totalBytes = sizeof(Node) + pad + bufBytes;
      rawPtr = allocator_.allocate(totalBytes);
    }

    // 3) compute final addresses outside lock
    uintptr_t newBase     = reinterpret_cast<uintptr_t>(rawPtr);
    uintptr_t payloadAddr = newBase + sizeof(Node) + pad;
    uintptr_t nodeAddr    = payloadAddr - sizeof(Node);

    // 4) construct node and return
    auto* node = reinterpret_cast<Node*>(nodeAddr);
    node->next = nullptr;
    return node;
  }

  ftl::BumpAllocator&              allocator_;
  ftl::Mutex                       mutex_;
  etl::array<Node*, NUM_SLOTS>     heads_;
};

} // namespace ftl
