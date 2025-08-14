#pragma once

#include <cstdlib>
#include <new>
#include "strategy.hpp"
#include "ftl/buffer.hpp"

namespace ftl::allocator {

/// Malloc-based implementation of IBufferStrategy
/// Allocates ftl::Buffer objects of a single size using malloc/free
class MallocBufferStrategy : public IBufferStrategy {
private:
    using Base = IBufferStrategy;
    
public:
    /// Constructor for single-size buffer strategy
    /// @param size Size of buffers to allocate
    /// @param alignment Alignment requirement for buffers (defaults to max_align_t)
    explicit MallocBufferStrategy(std::size_t size,
                                  std::size_t alignment = alignof(std::max_align_t)) noexcept
        : Base(size, alignment) {}
    
    /// Allocate a Buffer of the configured size
    /// @param req_size Requested size (must be <= size_)
    /// @return Pointer to allocated Buffer, or nullptr on failure
    ftl::Buffer* allocate(std::size_t req_size) noexcept override {
        // Check size is within our allocation size
        if (req_size > size_) {
            return nullptr;
        }
        
        // Allocate memory for the data with proper alignment
        // Round up size to multiple of alignment as required by aligned_alloc
        std::size_t aligned_size = (size_ + alignment_ - 1) & ~(alignment_ - 1);
        uint8_t* data = static_cast<uint8_t*>(std::aligned_alloc(alignment_, aligned_size));
        if (!data) {
            return nullptr;  // allocation failed
        }
        
        // Allocate the Buffer object itself
        void* buffer_mem = std::malloc(sizeof(ftl::Buffer));
        if (!buffer_mem) {
            std::free(data);  // Clean up data allocation
            return nullptr;
        }
        
        // Construct the Buffer in-place with requested size
        return new (buffer_mem) ftl::Buffer(data, req_size);
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