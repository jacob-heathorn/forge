#pragma once

#include <cstddef>
#include <new>            // for placement new
#include <utility>        // for std::forward

#include "ftl/bump_allocator.hpp"
#include "ftl/mutex.hpp"


namespace ftl {

//-------------------------------------------------------------------------------------------------
// BumpPool (mutex-protected): fixed-capacity object pool backed by a bump allocator.
// 
// Thread safety is now provided by a single mutex guarding both the free-list and
// bump-allocator growth.  No atomics are used.
//-------------------------------------------------------------------------------------------------
template <typename T>
class BumpPool {
public:
  // Construct a pool that preallocates 'initialSize' objects.
  // 'allocator' must outlive this pool.
  BumpPool(ftl::BumpAllocator& allocator, std::size_t initialSize = 1)
    : allocator_(allocator)
  {
    ftl::LockGuard<ftl::Mutex> lock(mutex_);
    for (std::size_t i = 0; i < initialSize; ++i) {
      void* mem = allocator_.allocate(sizeof(Node));
      ++total_count_;
      auto* node = new (mem) Node{};
      node->next = head_;
      head_ = node;
      ++free_count_;
    }
  }

  ~BumpPool() = default;

  // non-copyable, non-movable
  BumpPool(const BumpPool&) = delete;
  BumpPool& operator=(const BumpPool&) = delete;
  BumpPool(BumpPool&&) = delete;
  BumpPool& operator=(BumpPool&&) = delete;

  // Query capacities (thread-safe)
  std::size_t TotalSize() const noexcept {
    ftl::LockGuard<ftl::Mutex> lock(mutex_);
    return total_count_;
  }
  std::size_t FreeSize() const noexcept {
    ftl::LockGuard<ftl::Mutex> lock(mutex_);
    return free_count_;
  }
  std::size_t UsedSize() const noexcept {
    ftl::LockGuard<ftl::Mutex> lock(mutex_);
    return total_count_ - free_count_;
  }

  // Acquire an object, constructing it in-place.
  // If the free-list is non-empty, pop a node; otherwise bump-allocate.
  template <typename... Args>
  T* acquire(Args&&... args) {
    Node* node = nullptr;
    {
      ftl::LockGuard<ftl::Mutex> lock(mutex_);
      if (head_) {
        node    = head_;
        head_   = node->next;
        --free_count_;
      } else {
        // grow pool
        void* mem = allocator_.allocate(sizeof(Node));
        ++total_count_;
        node = new (mem) Node{};
      }
    }
    // placement-new the T inside our node (outside lock to minimize contention)
    return ::new (&node->storage) T(std::forward<Args>(args)...);
  }

  // Release an object back into the pool.
  // Destroys the T, then pushes the node onto the free-list.
  void release(T* obj) noexcept {
    if (!obj) return;
    // call destructor outside lock
    obj->~T();

    // recover our Node* (storage is first member)
    Node* node = reinterpret_cast<Node*>(
      reinterpret_cast<unsigned char*>(obj)
        - offsetof(Node, storage)
    );

    ftl::LockGuard<ftl::Mutex> lock(mutex_);
    node->next = head_;
    head_      = node;
    ++free_count_;
  }

private:
  // Single-linked list node: just a next pointer + storage for T
  struct Node {
    Node* next{nullptr};
    alignas(T) unsigned char storage[sizeof(T)];
  };

  ftl::BumpAllocator& allocator_;
  mutable ftl::Mutex   mutex_;        // guards head_, counts, and allocator growth

  Node*        head_{nullptr};        // free-list head
  std::size_t  total_count_{0};       // number of nodes ever allocated
  std::size_t  free_count_{0};        // number of nodes currently in free-list
};

}