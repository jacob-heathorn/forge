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

// Base interface for buffer allocations with multiple size classes
// Allocates ftl::Buffer* pointers from the smallest suitable size class
template <std::size_t NUM_SLOTS>
class IBufferStrategy {
protected:
    std::array<std::size_t, NUM_SLOTS> sizes_;  // Size classes (sorted)
    
public:
    // Constructor taking an array of sizes (will be sorted automatically)
    explicit IBufferStrategy(const std::array<std::size_t, NUM_SLOTS>& sizes) noexcept 
        : sizes_(sizes) {
        static_assert(NUM_SLOTS > 0, "IBufferStrategy requires at least one size class");
        // Sort sizes for efficient lookup
        std::sort(sizes_.begin(), sizes_.end());
    }
    
    virtual ~IBufferStrategy() = default;
    
    // Allocate a buffer of the smallest size class >= requested size
    // Returns a pointer to ftl::Buffer or nullptr on failure
    virtual ftl::Buffer* allocate(std::size_t req_size) noexcept = 0;
    
    // Deallocate a buffer
    virtual void deallocate(ftl::Buffer* buffer) noexcept = 0;
    
    // Get the number of size classes
    static constexpr std::size_t num_slots() noexcept { return NUM_SLOTS; }
    
    // Get the sizes array
    const std::array<std::size_t, NUM_SLOTS>& sizes() const noexcept { return sizes_; }
    
    // Get a specific size
    std::size_t size(std::size_t idx) const noexcept {
        return (idx < NUM_SLOTS) ? sizes_[idx] : 0;
    }
};

}
