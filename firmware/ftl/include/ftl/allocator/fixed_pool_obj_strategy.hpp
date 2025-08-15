#pragma once

#include <cstddef>
#include <new>
#include "strategy.hpp"
#include "ftl/allocator/bump_allocator.hpp"
#include "ftl/mutex.hpp"

namespace ftl::allocator {

/// Fixed-size pool object allocation strategy
/// Pre-allocates a fixed number of nodes from a bump allocator
/// @tparam T Type of objects to allocate
template <typename T>
class FixedPoolObjStrategy : public IObjStrategy<T> {
private:
    // Node structure for the free list
    struct Node {
        Node* next{nullptr};
        alignas(T) std::byte storage[sizeof(T)];
    };
    
    mutable Mutex mutex_;         // Protects free list
    Node* free_list_{nullptr};    // Head of free list
    Node* pool_start_{nullptr};   // Start of allocated pool
    Node* pool_end_{nullptr};     // End of allocated pool
    std::size_t capacity_{0};     // Number of nodes allocated
    
public:
    /// Constructor that pre-allocates nodes from bump allocator
    /// @param allocator Bump allocator to allocate nodes from
    /// @param count Number of nodes to pre-allocate
    FixedPoolObjStrategy(BumpAllocator& allocator, std::size_t count) noexcept
        : capacity_(count) {
        
        if (count == 0) {
            return;
        }
        
        // Allocate all nodes at once as a contiguous block
        std::size_t total_size = sizeof(Node) * count;
        void* mem = allocator.allocate(total_size, alignof(Node));
        
        if (!mem) {
            capacity_ = 0;
            return;
        }
        
        // Set up pool boundaries
        pool_start_ = static_cast<Node*>(mem);
        pool_end_ = pool_start_ + count;
        
        // Initialize free list with all nodes
        Node* nodes = pool_start_;
        for (std::size_t i = 0; i < count - 1; ++i) {
            nodes[i].next = &nodes[i + 1];
        }
        nodes[count - 1].next = nullptr;
        free_list_ = &nodes[0];
    }
    
    /// Allocate memory for an object
    /// @return Pointer to allocated memory, or nullptr if pool is exhausted
    void* allocate() noexcept override {
        LockGuard<Mutex> lock(mutex_);
        
        if (!free_list_) {
            return nullptr;  // Pool exhausted
        }
        
        Node* node = free_list_;
        free_list_ = node->next;
        return node->storage;
    }
    
    /// Deallocate memory for an object
    /// @param ptr Pointer to memory to deallocate
    void deallocate(void* ptr) noexcept override {
        if (!ptr) return;
        
        // Calculate node address from storage pointer
        // storage is at offset of sizeof(Node*) from Node start
        Node* node = reinterpret_cast<Node*>(
            static_cast<std::byte*>(ptr) - offsetof(Node, storage)
        );
        
        // Validate that ptr is from our pool
        if (node < pool_start_ || node >= pool_end_) {
            // Invalid pointer - not from our pool
            return;
        }
        
        LockGuard<Mutex> lock(mutex_);
        
        // Add back to free list
        node->next = free_list_;
        free_list_ = node;
    }
    
    /// Get the maximum number of objects that can be allocated
    std::size_t capacity() const noexcept {
        return capacity_;
    }
    
    /// Get the number of currently available objects
    std::size_t available() const noexcept {
        LockGuard<Mutex> lock(mutex_);
        
        std::size_t count = 0;
        Node* current = free_list_;
        while (current) {
            ++count;
            current = current->next;
        }
        return count;
    }
    
    /// Get the number of currently allocated objects
    std::size_t allocated() const noexcept {
        return capacity() - available();
    }
};

} // namespace ftl::allocator