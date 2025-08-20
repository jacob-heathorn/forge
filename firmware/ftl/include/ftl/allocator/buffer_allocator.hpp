#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <algorithm>
#include "strategy.hpp"
#include "ftl/buffer.hpp"

namespace ftl::allocator {

/// Allocator that manages buffers using multiple IBufferStrategy instances
/// Each strategy handles a specific buffer size
class BufferAllocator {
public:
    static constexpr std::size_t kMaxSlots = 8;  // Maximum number of strategies/sizes
    
private:
    std::array<IBufferStrategy*, kMaxSlots> strategies_{};  // Pointers to strategies (sorted by size)
    std::array<std::size_t, kMaxSlots> sizes_{};           // Sizes (sorted)
    std::size_t num_slots_;                                 // Actual number of slots in use
    
public:
    /// Variadic template constructor accepting strategy references
    /// Strategies will be sorted by their size for efficient allocation
    /// @param strategies Strategy references (must outlive this allocator)
    template <typename... Strategies>
    explicit BufferAllocator(Strategies&... strategies) noexcept
        : num_slots_(sizeof...(strategies)) {
        static_assert(sizeof...(strategies) <= kMaxSlots, "Number of strategies exceeds maximum");
        
        // Initialize all slots to nullptr/0
        strategies_.fill(nullptr);
        sizes_.fill(0);
        
        // Helper to populate arrays
        std::size_t idx = 0;
        ((strategies_[idx] = &strategies, sizes_[idx] = strategies.size(), ++idx), ...);
        
        // Sort strategies by size - simple bubble sort to avoid std::sort issues
        // with aggressive optimization
        for (std::size_t i = 0; i < num_slots_; ++i) {
            for (std::size_t j = i + 1; j < num_slots_; ++j) {
                if (sizes_[j] < sizes_[i]) {
                    // Swap strategies
                    IBufferStrategy* temp_strategy = strategies_[i];
                    strategies_[i] = strategies_[j];
                    strategies_[j] = temp_strategy;
                    
                    // Swap sizes
                    std::size_t temp_size = sizes_[i];
                    sizes_[i] = sizes_[j];
                    sizes_[j] = temp_size;
                }
            }
        }
    }
    
    /// Allocate a buffer of the smallest size >= requested size
    /// @param req_size Minimum size needed
    /// @return Pointer to allocated Buffer, or nullptr on failure
    ftl::Buffer* allocate(std::size_t req_size) noexcept {
        // Find the smallest strategy that can handle this size
        for (std::size_t i = 0; i < num_slots_; ++i) {
            if (sizes_[i] >= req_size) {
                return strategies_[i]->allocate(req_size);
            }
        }
        return nullptr;  // No strategy can handle this size
    }
    
    /// Deallocate a buffer
    /// @param buffer Pointer to Buffer to deallocate
    void deallocate(ftl::Buffer* buffer) noexcept {
        if (!buffer) return;
        
        // Find the strategy that would have allocated this buffer
        // based on the buffer's size (same logic as allocate)
        std::size_t buffer_size = buffer->size();
        for (std::size_t i = 0; i < num_slots_; ++i) {
            if (sizes_[i] >= buffer_size) {
                strategies_[i]->deallocate(buffer);
                return;
            }
        }
        
        // If we get here, no strategy could have allocated this buffer
        // This shouldn't happen in normal operation
    }
    
    /// Get the number of size classes
    std::size_t num_slots() const noexcept { 
        return num_slots_; 
    }
    
    /// Get the sizes array (all kMaxSlots elements, use num_slots() for actual count)
    const std::array<std::size_t, kMaxSlots>& sizes() const noexcept { 
        return sizes_; 
    }
    
    /// Get a specific size
    std::size_t size(std::size_t idx) const noexcept {
        return (idx < num_slots_) ? sizes_[idx] : 0;
    }
    
    /// Get the alignment requirement of a specific strategy
    std::size_t alignment(std::size_t idx) const noexcept { 
        return (idx < num_slots_ && strategies_[idx]) ? strategies_[idx]->alignment() : 0;
    }
};

}  // namespace ftl::allocator