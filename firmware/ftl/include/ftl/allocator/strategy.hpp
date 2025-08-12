#pragma once

#include <cstddef>
#include <cstdint>

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

}
