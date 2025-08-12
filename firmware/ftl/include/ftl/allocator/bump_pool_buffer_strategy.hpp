#pragma once

#include <cstddef>
#include <new>
#include "strategy.hpp"
#include "ftl/buffer.hpp"
#include "ftl/bump_allocator.hpp"
#include "ftl/mutex.hpp"

namespace ftl::allocator {

/// Bump pool-based implementation of IBufferStrategy
/// Allocates ftl::Buffer objects from a bump allocator with free list reuse
template <std::size_t NUM_SLOTS>
class BumpPoolBufferStrategy : public IBufferStrategy<NUM_SLOTS> {
private:
    using Base = IBufferStrategy<NUM_SLOTS>;
    
    // Node for free list management
    struct BufferNode {
        BufferNode* next;
        ftl::Buffer buffer;
    };
    
    BumpAllocator& allocator_;
    mutable Mutex mutex_;  // Protects free lists
    
    // Free lists for each size class
    std::array<BufferNode*, NUM_SLOTS> free_lists_{};
    
    /// Find the index of a specific size in the sorted sizes array
    std::size_t find_size_index(std::size_t size) const noexcept {
        for (std::size_t i = 0; i < NUM_SLOTS; ++i) {
            if (this->sizes_[i] == size) {
                return i;
            }
        }
        return NUM_SLOTS;  // Not found
    }
    
public:
    /// Constructor taking a bump allocator, sizes array, and optional alignment
    /// @param allocator BumpAllocator to use (must outlive this strategy)
    /// @param sizes Array of buffer sizes (will be sorted by base class)
    /// @param alignment Alignment requirement for buffers (defaults to max_align_t)
    BumpPoolBufferStrategy(BumpAllocator& allocator,
                           const std::array<std::size_t, NUM_SLOTS>& sizes,
                           std::size_t alignment = alignof(std::max_align_t)) noexcept
        : Base(sizes, alignment), allocator_(allocator) {
        // Initialize free lists to nullptr
        for (auto& list : free_lists_) {
            list = nullptr;
        }
    }
    
    /// Allocate a Buffer of the smallest size >= requested size
    /// @param req_size Minimum size needed
    /// @return Pointer to allocated Buffer, or nullptr on failure
    ftl::Buffer* allocate(std::size_t req_size) noexcept override {
        // Find the smallest size that fits (sizes_ is already sorted by base class)
        std::size_t alloc_size = 0;
        std::size_t size_index = NUM_SLOTS;
        
        for (std::size_t i = 0; i < NUM_SLOTS; ++i) {
            if (this->sizes_[i] >= req_size) {
                alloc_size = this->sizes_[i];
                size_index = i;
                break;
            }
        }
        
        if (alloc_size == 0) {
            return nullptr;  // No size class large enough
        }
        
        LockGuard<Mutex> lock(mutex_);
        
        // Check if we have a buffer in the free list for this size
        if (free_lists_[size_index]) {
            BufferNode* node = free_lists_[size_index];
            free_lists_[size_index] = node->next;
            // Return the buffer pointer (already constructed)
            return &node->buffer;
        }
        
        // Allocate new from bump allocator
        // Need space for BufferNode which includes the Buffer object
        void* node_mem = allocator_.allocate(sizeof(BufferNode), alignof(BufferNode));
        if (!node_mem) {
            return nullptr;  // Out of memory
        }
        
        // Allocate the actual buffer data with proper alignment
        // Round up size to multiple of alignment as needed
        std::size_t aligned_size = (alloc_size + this->alignment_ - 1) & ~(this->alignment_ - 1);
        void* data = allocator_.allocate(aligned_size, this->alignment_);
        if (!data) {
            return nullptr;  // Out of memory
        }
        
        // Construct the BufferNode with the Buffer
        BufferNode* node = new (node_mem) BufferNode;
        node->next = nullptr;
        // Construct the Buffer in-place
        new (&node->buffer) ftl::Buffer(static_cast<uint8_t*>(data), alloc_size);
        
        return &node->buffer;
    }
    
    /// Deallocate a buffer (adds to free list for reuse)
    /// @param buffer Pointer to Buffer to deallocate
    void deallocate(ftl::Buffer* buffer) noexcept override {
        if (!buffer) return;
        
        // Find which size class this buffer belongs to
        std::size_t size_index = find_size_index(buffer->size());
        if (size_index >= NUM_SLOTS) {
            return;  // Invalid size, shouldn't happen
        }
        
        // Get the containing BufferNode
        // The Buffer is embedded in BufferNode at offset of BufferNode::buffer
        BufferNode* node = reinterpret_cast<BufferNode*>(
            reinterpret_cast<char*>(buffer) - offsetof(BufferNode, buffer)
        );
        
        LockGuard<Mutex> lock(mutex_);
        
        // Add to free list
        node->next = free_lists_[size_index];
        free_lists_[size_index] = node;
    }
};

}  // namespace ftl::allocator