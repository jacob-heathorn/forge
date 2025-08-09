#pragma once

#include <cstddef>
#include <new>
#include "ftl/allocation_strategy.hpp"
#include "ftl/bump_allocator.hpp"
#include "ftl/mutex.hpp"

namespace ftl {

/// Pool-based allocation strategy that reuses memory from a free list
/// When the free list is empty, allocates new memory from a BumpAllocator
/// @tparam T Type of objects to allocate
template<typename T>
class BumpPoolAllocationStrategy : public AllocationStrategy<T> {
public:
    /// Construct a pool allocation strategy with initial capacity
    /// @param allocator BumpAllocator to use for backing memory (must outlive this object)
    /// @param initial_size Number of objects to pre-allocate
    BumpPoolAllocationStrategy(ftl::BumpAllocator& allocator, std::size_t initial_size = 1)
        : allocator_(allocator) {
        ftl::LockGuard<ftl::Mutex> lock(mutex_);
        
        // Pre-allocate initial_size nodes
        for (std::size_t i = 0; i < initial_size; ++i) {
            void* mem = allocator_.allocate(sizeof(Node));
            if (!mem) {
                break;  // Stop if out of memory
            }
            ++total_count_;
            
            Node* node = reinterpret_cast<Node*>(mem);
            node->next = head_;
            head_ = node;
            ++free_count_;
        }
    }

    ~BumpPoolAllocationStrategy() override = default;

    // Non-copyable, non-movable
    BumpPoolAllocationStrategy(const BumpPoolAllocationStrategy&) = delete;
    BumpPoolAllocationStrategy& operator=(const BumpPoolAllocationStrategy&) = delete;
    BumpPoolAllocationStrategy(BumpPoolAllocationStrategy&&) = delete;
    BumpPoolAllocationStrategy& operator=(BumpPoolAllocationStrategy&&) = delete;

    /// Allocate raw memory for an object of type T
    /// Does NOT construct the object - caller must use placement new
    /// @return Pointer to allocated memory, or nullptr if allocation fails
    T* allocate() override {
        ftl::LockGuard<ftl::Mutex> lock(mutex_);
        
        Node* node = nullptr;
        
        if (head_) {
            // Reuse from free list
            node = head_;
            head_ = node->next;
            --free_count_;
        } else {
            // Allocate new node from bump allocator
            void* mem = allocator_.allocate(sizeof(Node));
            if (!mem) {
                return nullptr;  // Out of memory
            }
            ++total_count_;
            node = reinterpret_cast<Node*>(mem);
        }
        
        // Return pointer to storage area (raw memory for T)
        return reinterpret_cast<T*>(&node->storage);
    }

    /// Deallocate raw memory for an object of type T
    /// Object should already be destructed before calling this
    /// @param ptr Pointer to memory to deallocate
    void deallocate(T* ptr) noexcept override {
        if (!ptr) return;
        
        // Calculate Node address from T* pointer
        // Since storage is the first member after next pointer, we need to go back
        Node* node = reinterpret_cast<Node*>(
            reinterpret_cast<unsigned char*>(ptr) - offsetof(Node, storage)
        );
        
        ftl::LockGuard<ftl::Mutex> lock(mutex_);
        
        // Add to free list
        node->next = head_;
        head_ = node;
        ++free_count_;
    }

    /// Get total number of objects allocated from the pool
    std::size_t total_size() const {
        ftl::LockGuard<ftl::Mutex> lock(mutex_);
        return total_count_;
    }

    /// Get number of objects currently in use
    std::size_t used_size() const {
        ftl::LockGuard<ftl::Mutex> lock(mutex_);
        return total_count_ - free_count_;
    }

    /// Get number of objects available in free list
    std::size_t free_size() const {
        ftl::LockGuard<ftl::Mutex> lock(mutex_);
        return free_count_;
    }

private:
    // Node structure for free list
    struct Node {
        Node* next{nullptr};
        alignas(T) unsigned char storage[sizeof(T)];
    };

    ftl::BumpAllocator& allocator_;
    mutable ftl::Mutex mutex_;      // Protects free list and counts
    
    Node* head_{nullptr};           // Head of free list
    std::size_t total_count_{0};    // Total nodes allocated
    std::size_t free_count_{0};     // Nodes in free list
};

}  // namespace ftl