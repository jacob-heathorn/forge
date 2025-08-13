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
class IBufferStrategy {
public:
    static constexpr std::size_t kMaxSlots = 8;  // Maximum number of size classes
    
protected:
    std::array<std::size_t, kMaxSlots> sizes_;   // Size classes (sorted)
    std::size_t num_slots_;                      // Actual number of slots in use
    std::size_t alignment_;                      // Alignment requirement for buffers
    
public:
    // Templated constructor accepting std::array
    // @param sizes std::array of buffer sizes (will be sorted automatically)
    // @param alignment Alignment requirement for allocated buffers
    template <std::size_t N>
    IBufferStrategy(const std::array<std::size_t, N>& sizes,
                    std::size_t alignment = alignof(std::max_align_t)) noexcept 
        : num_slots_(N), alignment_(alignment) {
        static_assert(N <= kMaxSlots, "Number of size classes exceeds maximum");
        // Initialize all slots to 0
        sizes_.fill(0);
        // Copy provided sizes
        for (std::size_t i = 0; i < N; ++i) {
            sizes_[i] = sizes[i];
        }
        // Sort only the used slots
        std::sort(sizes_.begin(), sizes_.begin() + N);
    }
    
    virtual ~IBufferStrategy() = default;
    
    // Allocate a buffer of the smallest size class >= requested size
    // Returns a pointer to ftl::Buffer or nullptr on failure
    virtual ftl::Buffer* allocate(std::size_t req_size) noexcept = 0;
    
    // Deallocate a buffer
    virtual void deallocate(ftl::Buffer* buffer) noexcept = 0;
    
    // Get the number of size classes in use
    std::size_t num_slots() const noexcept { return num_slots_; }
    
    // Get the sizes array (all kMaxSlots elements, use num_slots() for actual count)
    const std::array<std::size_t, kMaxSlots>& sizes() const noexcept { return sizes_; }
    
    // Get a specific size
    std::size_t size(std::size_t idx) const noexcept {
        return (idx < num_slots_) ? sizes_[idx] : 0;
    }
    
    // Get the alignment requirement
    std::size_t alignment() const noexcept { return alignment_; }
};

}
