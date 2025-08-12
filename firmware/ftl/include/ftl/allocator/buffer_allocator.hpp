#pragma once

#include <cstddef>
#include <cstdint>
#include "strategy.hpp"
#include "ftl/buffer.hpp"

namespace ftl::allocator {

/// Allocator that manages buffers using an IBufferStrategy
/// This is a simple wrapper that delegates to the strategy
template <std::size_t NUM_SLOTS>
class BufferAllocator {
private:
    IBufferStrategy<NUM_SLOTS>& strategy_;
    
public:
    /// Constructor taking a reference to a buffer strategy
    /// @param strategy Buffer strategy to use (must outlive this allocator)
    explicit BufferAllocator(IBufferStrategy<NUM_SLOTS>& strategy) noexcept
        : strategy_(strategy) {}
    
    /// Allocate a buffer of the smallest size >= requested size
    /// @param req_size Minimum size needed
    /// @return Pointer to allocated Buffer, or nullptr on failure
    ftl::Buffer* allocate(std::size_t req_size) noexcept {
        return strategy_.allocate(req_size);
    }
    
    /// Deallocate a buffer
    /// @param buffer Pointer to Buffer to deallocate
    void deallocate(ftl::Buffer* buffer) noexcept {
        strategy_.deallocate(buffer);
    }
    
    /// Get the number of size classes
    static constexpr std::size_t num_slots() noexcept { 
        return NUM_SLOTS; 
    }
    
    /// Get the sizes array from the strategy
    const std::array<std::size_t, NUM_SLOTS>& sizes() const noexcept { 
        return strategy_.sizes(); 
    }
    
    /// Get a specific size
    std::size_t size(std::size_t idx) const noexcept {
        return strategy_.size(idx);
    }
    
    /// Get the alignment requirement
    std::size_t alignment() const noexcept { 
        return strategy_.alignment(); 
    }
};

}  // namespace ftl::allocator
