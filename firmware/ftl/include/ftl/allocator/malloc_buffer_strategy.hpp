#pragma once

#include <cstdlib>
#include <new>
#include "strategy.hpp"
#include "ftl/buffer.hpp"

namespace ftl::allocator {

/// Malloc-based implementation of IBufferStrategy
/// Allocates ftl::Buffer objects using malloc/free
template <std::size_t NUM_SLOTS>
class MallocBufferStrategy : public IBufferStrategy<NUM_SLOTS> {
private:
    using Base = IBufferStrategy<NUM_SLOTS>;
    
public:
    /// Constructor taking an array of buffer sizes and optional alignment
    /// @param sizes Array of buffer sizes (will be sorted by base class)
    /// @param alignment Alignment requirement for buffers (defaults to max_align_t)
    explicit MallocBufferStrategy(const std::array<std::size_t, NUM_SLOTS>& sizes,
                                  std::size_t alignment = alignof(std::max_align_t)) noexcept
        : Base(sizes, alignment) {}
    
    /// Allocate a Buffer of the smallest size >= requested size
    /// @param req_size Minimum size needed
    /// @return Pointer to allocated Buffer, or nullptr on failure
    ftl::Buffer* allocate(std::size_t req_size) noexcept override {
        // Find the smallest size that fits (sizes_ is already sorted by base class)
        std::size_t alloc_size = 0;
        for (std::size_t size : this->sizes_) {
            if (size >= req_size) {
                alloc_size = size;
                break;
            }
        }
        
        if (alloc_size == 0) {
            return nullptr;  // No size class large enough
        }
        
        // Allocate memory for the data with proper alignment
        // Round up size to multiple of alignment as required by aligned_alloc
        std::size_t aligned_size = (alloc_size + this->alignment_ - 1) & ~(this->alignment_ - 1);
        uint8_t* data = static_cast<uint8_t*>(std::aligned_alloc(this->alignment_, aligned_size));
        if (!data) {
            return nullptr;  // allocation failed
        }
        
        // Allocate the Buffer object itself
        void* buffer_mem = std::malloc(sizeof(ftl::Buffer));
        if (!buffer_mem) {
            std::free(data);  // Clean up data allocation
            return nullptr;
        }
        
        // Construct the Buffer in-place
        return new (buffer_mem) ftl::Buffer(data, alloc_size);
    }
    
    /// Deallocate a Buffer
    /// @param buffer Pointer to Buffer to deallocate
    void deallocate(ftl::Buffer* buffer) noexcept override {
        if (!buffer) return;
        
        // Free the data pointed to by the buffer
        if (buffer->front()) {
            std::free(buffer->front());
        }
        
        // Destroy and free the Buffer object itself
        buffer->~Buffer();
        std::free(buffer);
    }
};

}  // namespace ftl::allocator