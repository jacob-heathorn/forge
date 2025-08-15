#pragma once

#include <cstddef>
#include <new>
#include "strategy.hpp"
#include "ftl/allocator/bump_allocator.hpp"
#include "ftl/mutex.hpp"

namespace ftl::allocator {

namespace detail {

// Non-virtual fixed pool implementation to avoid code duplication
// Pre-allocates all nodes from bump allocator
class FixedPoolImpl {
private:
    // Base node structure for the free list
    struct NodeBase {
        NodeBase* next{nullptr};
    };
    
    mutable Mutex mutex_;         // Protects free list
    NodeBase* free_list_{nullptr};  // Head of free list
    std::size_t node_size_;       // Total size of node including header and storage
    std::size_t storage_offset_;  // Offset from node start to storage
    std::size_t capacity_{0};     // Number of nodes allocated
    
public:
    FixedPoolImpl(BumpAllocator& allocator, std::size_t size, std::size_t alignment, std::size_t count) noexcept
        : capacity_(count) {
        
        if (count == 0) {
            return;
        }
        
        // Calculate storage offset to ensure proper alignment
        // Storage comes after NodeBase, aligned to requested alignment
        storage_offset_ = sizeof(NodeBase);
        if (alignment > alignof(NodeBase)) {
            storage_offset_ = (storage_offset_ + alignment - 1) & ~(alignment - 1);
        }
        node_size_ = storage_offset_ + size;
        
        // Allocate all nodes at once as a contiguous block
        std::size_t total_size = node_size_ * count;
        void* mem = allocator.allocate(total_size, alignment);
        
        if (!mem) {
            capacity_ = 0;
            return;
        }
        
        // Initialize free list with all nodes
        unsigned char* nodes = static_cast<unsigned char*>(mem);
        for (std::size_t i = 0; i < count - 1; ++i) {
            NodeBase* node = reinterpret_cast<NodeBase*>(nodes + i * node_size_);
            NodeBase* next = reinterpret_cast<NodeBase*>(nodes + (i + 1) * node_size_);
            node->next = next;
        }
        NodeBase* last = reinterpret_cast<NodeBase*>(nodes + (count - 1) * node_size_);
        last->next = nullptr;
        free_list_ = reinterpret_cast<NodeBase*>(nodes);
    }
    
    void* allocate() noexcept {
        LockGuard<Mutex> lock(mutex_);
        
        if (!free_list_) {
            return nullptr;  // Pool exhausted
        }
        
        NodeBase* node = free_list_;
        free_list_ = node->next;
        // Return pointer to storage area
        return reinterpret_cast<unsigned char*>(node) + storage_offset_;
    }
    
    void deallocate(void* ptr) noexcept {
        if (!ptr) return;
        
        // Calculate Node address from storage pointer
        NodeBase* node = reinterpret_cast<NodeBase*>(
            static_cast<unsigned char*>(ptr) - storage_offset_
        );
        
        LockGuard<Mutex> lock(mutex_);
        
        // Add back to free list
        node->next = free_list_;
        free_list_ = node;
    }
    
    std::size_t capacity() const noexcept {
        return capacity_;
    }
    
    std::size_t available() const noexcept {
        LockGuard<Mutex> lock(mutex_);
        
        std::size_t count = 0;
        NodeBase* current = free_list_;
        while (current) {
            ++count;
            current = current->next;
        }
        return count;
    }
};

} // namespace detail

/// Fixed-size pool object allocation strategy
/// Pre-allocates a fixed number of nodes from a bump allocator
/// @tparam T Type of objects to allocate
template <typename T>
class FixedPoolObjStrategy : public IObjStrategy<T> {
private:
    detail::FixedPoolImpl impl_;
    
public:
    /// Constructor that pre-allocates nodes from bump allocator
    /// @param allocator Bump allocator to allocate nodes from
    /// @param count Number of nodes to pre-allocate
    FixedPoolObjStrategy(BumpAllocator& allocator, std::size_t count) noexcept
        : impl_(allocator, sizeof(T), alignof(T), count) {}
    
    /// Allocate memory for an object
    /// @return Pointer to allocated memory, or nullptr if pool is exhausted
    void* allocate() noexcept override {
        return impl_.allocate();
    }
    
    /// Deallocate memory for an object
    /// @param ptr Pointer to memory to deallocate
    void deallocate(void* ptr) noexcept override {
        impl_.deallocate(ptr);
    }
    
    /// Get the maximum number of objects that can be allocated
    std::size_t capacity() const noexcept {
        return impl_.capacity();
    }
    
    /// Get the number of currently available objects
    std::size_t available() const noexcept {
        return impl_.available();
    }
    
    /// Get the number of currently allocated objects
    std::size_t allocated() const noexcept {
        return capacity() - available();
    }
};

} // namespace ftl::allocator