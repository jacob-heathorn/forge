#pragma once

#include <cstddef>
#include "strategy.hpp"
#include "ftl/allocator/bump_allocator.hpp"
#include "ftl/buffer.hpp"
#include "ftl/mutex.hpp"

namespace ftl::allocator {

/// Fixed-size pool buffer allocation strategy
/// Pre-allocates a fixed number of buffers from a bump allocator
class FixedPoolBufferStrategy : public IBufferStrategy {
private:
    // Node structure for the free list
    struct Node {
        Node* next{nullptr};
        Buffer buffer;
    };
    
    mutable Mutex mutex_;         // Protects free list
    Node* free_list_{nullptr};    // Head of free list
    std::size_t capacity_{0};     // Number of buffers allocated
    
public:
    /// Constructor that pre-allocates buffers from bump allocator
    /// @param allocator Bump allocator to allocate nodes from
    /// @param buffer_size Size of each buffer in bytes
    /// @param count Number of buffers to pre-allocate
    /// @param alignment Required alignment for buffers (default: alignof(std::max_align_t))
    FixedPoolBufferStrategy(BumpAllocator& allocator, std::size_t buffer_size, 
                           std::size_t count, std::size_t alignment = alignof(std::max_align_t)) noexcept
        : IBufferStrategy(buffer_size, alignment), capacity_(count) {
        
        if (count == 0 || buffer_size == 0) {
            capacity_ = 0;
            return;
        }
        
        // Calculate total size needed for all nodes
        // Each node contains: Node structure + aligned buffer data
        std::size_t node_size = sizeof(Node);
        std::size_t total_nodes_size = node_size * count;
        
        // Allocate nodes array
        void* nodes_mem = allocator.allocate(total_nodes_size, alignof(Node));
        if (!nodes_mem) {
            capacity_ = 0;
            return;
        }
        
        // Allocate buffer data as a separate contiguous block for better cache performance
        std::size_t total_buffer_size = buffer_size * count;
        void* buffer_mem = allocator.allocate(total_buffer_size, alignment);
        if (!buffer_mem) {
            capacity_ = 0;
            return;
        }
        
        // Initialize nodes and buffers
        Node* nodes = static_cast<Node*>(nodes_mem);
        uint8_t* buffer_data = static_cast<uint8_t*>(buffer_mem);
        
        for (std::size_t i = 0; i < count; ++i) {
            // Set up buffer for this node
            nodes[i].buffer = Buffer(buffer_data + (i * buffer_size), buffer_size);
            
            // Link into free list
            if (i < count - 1) {
                nodes[i].next = &nodes[i + 1];
            } else {
                nodes[i].next = nullptr;
            }
        }
        
        free_list_ = &nodes[0];
    }
    
    /// Allocate a buffer
    /// @param requested_size Size requested (must be <= size_)
    /// @return Pointer to allocated buffer, or nullptr if pool is exhausted
    Buffer* allocate(std::size_t requested_size) noexcept override {
        if (requested_size > size_) {
            return nullptr;  // Can't satisfy request
        }
        
        LockGuard<Mutex> lock(mutex_);
        
        if (!free_list_) {
            return nullptr;  // Pool exhausted
        }
        
        Node* node = free_list_;
        free_list_ = node->next;
        
        // Adjust buffer size to requested size
        node->buffer = Buffer(node->buffer.front(), requested_size);
        return &node->buffer;
    }
    
    /// Deallocate a buffer
    /// @param buffer Pointer to buffer to deallocate
    void deallocate(Buffer* buffer) noexcept override {
        if (!buffer) return;
        
        // Calculate node address from buffer pointer
        // Buffer is embedded in Node structure
        Node* node = reinterpret_cast<Node*>(
            reinterpret_cast<uint8_t*>(buffer) - offsetof(Node, buffer)
        );
        
        // No validation - trust that buffer came from our pool
        // This matches BumpPoolBufferStrategy behavior
        
        LockGuard<Mutex> lock(mutex_);
        
        // Add back to free list
        node->next = free_list_;
        free_list_ = node;
    }
    
    /// Get the maximum number of buffers that can be allocated
    std::size_t capacity() const noexcept {
        return capacity_;
    }
    
    /// Get the number of currently available buffers
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
    
    /// Get the number of currently allocated buffers
    std::size_t allocated() const noexcept {
        return capacity() - available();
    }
};

} // namespace ftl::allocator