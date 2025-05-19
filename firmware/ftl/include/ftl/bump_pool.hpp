#pragma once

#include <atomic>
#include <cstddef>
#include <new>       // for placement new
#include <utility>   // for std::forward

#include "ftl/bump_allocator.hpp"
#include "ftl/mutex.hpp"

//-------------------------------------------------------------------------------------------------
// BumpPool: a fast, fixed‑capacity object pool backed by a bump allocator.
// 
// Thread safety is achieved with two mechanisms:
//  1) A mutex around the bump‑allocation step (i.e. when grabbing new memory from the allocator).
//  2) A lock‑free singly‑linked free‑list (using atomics) for already‑allocated nodes, so that
//     acquire/release never blocks once the steady‑state pool is filled.
//
// In practice, you preallocate N objects up front. After that point, both `acquire()` and
// `release()` run purely on atomic push/pop, giving you deterministic, low‑latency behavior without
// context switches.
//
// Usage scenario: once your app has needed at most N live objects at once, it will never allocate
// again — all further gets/puts are lock‑free.
//
template <typename T>
class BumpPool {
 public:
  // Construct a pool that preallocates 'initialSize' objects.
  // 'alloc' must outlive this pool.
  BumpPool(ftl::BumpAllocator& allocator, std::size_t initialSize = 1)
    : allocator_(allocator)
  {
    // prebuild initialSize nodes
    for (std::size_t i = 0; i < initialSize; ++i) {
      // bump‑alloc under lock
      void* mem;
      {
        ftl::LockGuard<ftl::Mutex> lock(allocatorMutex_);
        mem = allocator_.allocate(sizeof(Node));
      }
      total_count_.fetch_add(1, std::memory_order_relaxed);
      auto* node = new (mem) Node{};
      pushNode_(node);
    }
  }
  ~BumpPool() = default;

  // No copying or moving.
  BumpPool(const BumpPool&) = delete;            // Delete copy constructor
  BumpPool& operator=(const BumpPool&) = delete; // Delete copy assignment operator
  BumpPool(BumpPool&&) = delete;                 // Delete move constructor
  BumpPool& operator=(BumpPool&&) = delete;      // Delete move assignment operator

  // Total nodes ever allocated (steady-state capacity)
  std::size_t TotalSize() const noexcept {
    return total_count_.load(std::memory_order_relaxed);
  }

  // Nodes currently in the free-list (available)
  std::size_t FreeSize() const noexcept {
    return free_count_.load(std::memory_order_relaxed);
  }

  // Nodes currently in use (acquired)
  std::size_t UsedSize() const noexcept {
    return TotalSize() - FreeSize();
  }

  // Acquire an object.  If the free‑list is non‑empty, pop it lock‑free.
  // Otherwise grab a fresh node under the mutex.
  template <typename... Args>
  T* acquire(Args&&... args) {
    Node* node = popNode_();
    if (!node) {
      // free‑list empty → bump‑alloc a new one
      void* mem;
      {
        ftl::LockGuard<ftl::Mutex> lock(allocatorMutex_);
        mem = allocator_.allocate(sizeof(Node));
      }
      total_count_.fetch_add(1, std::memory_order_relaxed);
      node = new (mem) Node{};
    }
    // placement‑new the T inside our node
    T* obj = ::new (&node->storage) T(std::forward<Args>(args)...);
    return obj;
  }

  // Release an object back into the pool.  Destroys the T and
  // pushes the node back lock‑free.
  void release(T* obj) noexcept {
    if (!obj) return;
    // call the destructor
    obj->~T();

    // recover our Node* (storage is first member, so pointer‑math is safe)
    Node* node = reinterpret_cast<Node*>(
      reinterpret_cast<unsigned char*>(obj) - offsetof(Node, storage)
    );
    pushNode_(node);
  }

 private:

  // Single‑linked list node; storage for T + next pointer
  struct Node {
    Node* next{nullptr};
    alignas(T) unsigned char storage[sizeof(T)];
  };

  // Pop head_ lock‑free, return nullptr if empty
  Node* popNode_() noexcept {
    // 1) Take a snapshot of the current head of the free-list
    Node* head = head_.load(std::memory_order_acquire);
    
    // 2) As long as there is at least one node in the list...
    while (head) {
      // 2a) Cache the next node pointer before we attempt removal
      Node* nxt = head->next;
  
      // 2b) Attempt to atomically replace head_ with its next node
      //    - expected: head (our snapshot)
      //    - desired:  nxt (the node after head)
      // On success (returns true): we have removed 'head' from the list
      // On failure (returns false): 'head' is updated to the new head_.retry
      if (head_.compare_exchange_weak(
            head,            // expected old head (updated on failure)
            nxt,             // new head we want to install
            std::memory_order_acquire,  // on success: acquire barrier
            std::memory_order_relaxed)) // on failure: no ordering needed
      {
        // 3) Successful pop: detach the node from the list
        head->next = nullptr;
        free_count_.fetch_sub(1, std::memory_order_relaxed);
        return head;
      }
      // Failure path: head now holds the latest head_.loop continues
    }
    
    // 4) Empty list: nothing to pop
    return nullptr;
  }

  // Push node onto head_ lock‑free
  void pushNode_(Node* node) noexcept {
    // 1) Grab a _snapshot_ of the current head of the free‑list.
    //    We don’t need any special ordering here, because we’re
    //    just preparing our new node’s next pointer.
    Node* head = head_.load(std::memory_order_relaxed);
  
    // 2) Loop until we successfully install `node` as the new head.
    do {
      // 2a) Point our new node at what used to be the head.
      //     At this point, `node->next` = snapshot-of-head.
      node->next = head;
  
      // 2b) Try to atomically swing head_ from the old `head` pointer
      //     to our `node`.  Two cases:
      //
      //     — Success:
      //         head_ still equals the snapshot in `head`,
      //         so we install `node`, return true,
      //         and exit the loop.
      //
      //     — Failure (could be spurious or because another thread
      //       raced in and changed head_):
      //         * compare_exchange_weak updates `head` with the
      //           current head_.load(), so that `head` now points
      //           at whoever “won” the race.
      //         * It returns false, so we fall through and loop again.
      //
    } while (!head_.compare_exchange_weak(
                 head,                // expected old head (updated on failure)
                 node,                // desired new head
                 std::memory_order_release,  // on success: publish node->next + prior writes
                 std::memory_order_relaxed)); // on failure: no extra fences
  
    // 3) At this point, `node` has been atomically installed
    //    as the new head of the free‑list, and anyone popping
    //    will see it (with acquire semantics on their side).
    free_count_.fetch_add(1, std::memory_order_relaxed);
  }

  static_assert(std::atomic<Node*>::is_always_lock_free,
    "Pointer atomics must be always lock-free");

  ftl::BumpAllocator&        allocator_;
  ftl::Mutex            allocatorMutex_;  // only used when bump‑allocating
  std::atomic<Node*>    head_{nullptr};   // free‑list head
  std::atomic<std::size_t>  total_count_{0};
  std::atomic<std::size_t>  free_count_{0};
};
