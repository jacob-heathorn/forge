#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include "ftl/buffer.hpp"

namespace ftl::allocator {

// ============================================================================
// Interface allocation strategies using virtual methods
// ============================================================================

// Base interface for block allocations (fixed-size blocks)
class IBlockStrategy {
protected:
    std::size_t size_;  // Size of blocks this strategy allocates
    
public:
    explicit IBlockStrategy(std::size_t size) noexcept : size_(size) {}
    virtual ~IBlockStrategy() = default;
    
    // Allocate a block with size and alignment baked into the implementation
    virtual void* allocate() noexcept = 0;
    
    // Deallocate a previously allocated block
    virtual void deallocate(void* ptr) noexcept = 0;
    
    // Get the block size this strategy allocates
    std::size_t size() const noexcept { return size_; }
};

// Base interface for object allocations (typed, single object)
template <typename T>
class IObjStrategy {
public:
    virtual ~IObjStrategy() = default;
    
    // Allocate storage for one T (size and alignment baked in)
    virtual void* allocate() noexcept = 0;
    
    // Deallocate storage for one T
    virtual void deallocate(void* ptr) noexcept = 0;
};

// Base interface for buffer allocations with a single fixed size
// Each strategy handles buffers of exactly one size
class IBufferStrategy {
protected:
    std::size_t size_;        // Size of buffers this strategy allocates
    std::size_t alignment_;   // Alignment requirement for buffers
    
public:
    // Constructor for single-size buffer strategy
    // @param size Size of buffers to allocate
    // @param alignment Alignment requirement for allocated buffers
    IBufferStrategy(std::size_t size,
                    std::size_t alignment = alignof(std::max_align_t)) noexcept 
        : size_(size), alignment_(alignment) {}
    
    virtual ~IBufferStrategy() = default;
    
    // Allocate a buffer of the configured size
    // @param req_size The requested size (must be <= size_)
    // Returns a pointer to ftl::Buffer or nullptr on failure
    virtual ftl::Buffer* allocate(std::size_t req_size) noexcept = 0;
    
    // Deallocate a buffer
    virtual void deallocate(ftl::Buffer* buffer) noexcept = 0;
    
    // Get the buffer size this strategy handles
    std::size_t size() const noexcept { return size_; }
    
    // Get the alignment requirement
    std::size_t alignment() const noexcept { return alignment_; }
};

}
