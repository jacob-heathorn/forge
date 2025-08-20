#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <new>
#include <cstddef>   // offsetof
#include "strategy.hpp"
#include "ftl/buffer.hpp"
#include "ftl/allocator/bump_allocator.hpp"
#include "ftl/mutex.hpp"

namespace ftl::allocator {

// Single-size bump pool buffer strategy
// Manages buffers of exactly one size with a free list for reuse
class BumpPoolBufferStrategy : public IBufferStrategy {
public:
    using Base = IBufferStrategy;

    struct BufferNode {
        BufferNode* next;
        ftl::Buffer buffer; // must keep standard-layout for offsetof
        
        BufferNode(uint8_t* data, std::size_t requested_size) noexcept
            : next(nullptr), buffer(data, requested_size) {}
    };
    static_assert(std::is_standard_layout<BufferNode>::value, "BufferNode must be standard-layout");

private:
    // Single mutex for this size's free list
    Mutex freelist_mutex_{};
    
    // Separate lock since allocator_ is NOT thread-safe
    Mutex alloc_mutex_;
    
    BumpAllocator& allocator_;
    
    BufferNode* free_list_ = nullptr;

public:
    // Constructor for single-size strategy
    BumpPoolBufferStrategy(BumpAllocator& allocator,
                          std::size_t size,
                          std::size_t alignment = alignof(std::max_align_t)) noexcept
        : Base(size, alignment)
        , allocator_(allocator) {}

    // Allocate with fast path free list reuse
    ftl::Buffer* allocate(std::size_t req_size) noexcept override {
        // Check size is within our allocation size
        if (req_size > size_) return nullptr;
        
        // Try reuse under a very short lock
        {
            LockGuard<Mutex> lk(freelist_mutex_);
            if (free_list_) {
                BufferNode* head = free_list_;
                free_list_ = head->next;
                head->buffer = ftl::Buffer(head->buffer.front(), req_size);  // Update size
                return &head->buffer;
            }
        }

        // Slow path: allocate BufferNode and data separately
        BufferNode* node;
        void* data;
        {
            // Only lock allocator if it isn't internally thread-safe.
            LockGuard<Mutex> lk(alloc_mutex_);
            // Allocate node
            node = static_cast<BufferNode*>(allocator_.allocate(sizeof(BufferNode), alignof(BufferNode)));
            if (!node) return nullptr;
            
            // Allocate data with requested alignment
            data = allocator_.allocate(size_, alignment_);
            if (!data) {
                // Can't deallocate node back to bump allocator, just leave it
                return nullptr;
            }
        }

        // Construct BufferNode with requested size
        new (node) BufferNode(static_cast<uint8_t*>(data), req_size);
        return &node->buffer;
    }

    // Deallocate with tiny critical section
    void deallocate(ftl::Buffer* buffer) noexcept override {
        if (!buffer) return;

        // Get node via standard-layout guarantee
        auto* node = reinterpret_cast<BufferNode*>(
            reinterpret_cast<char*>(buffer) - offsetof(BufferNode, buffer));

        // Push under a very short lock
        {
            LockGuard<Mutex> lk(freelist_mutex_);
            node->next = free_list_;
            free_list_ = node;
        }
    }
};

} // namespace ftl::allocator