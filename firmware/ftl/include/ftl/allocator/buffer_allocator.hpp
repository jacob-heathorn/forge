#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include "strategy.hpp"

namespace ftl::allocator {

/// A buffer handle that includes slot index information for efficient deallocation
struct Buffer {
    void* ptr;                    // Pointer to allocated memory
    std::size_t size;             // Size of the allocated block
    std::uint16_t slot_index;     // Index of the slot used
    
    /// Default constructor creates an invalid buffer
    Buffer() noexcept : ptr(nullptr), size(0), slot_index(0) {}
    
    /// Construct a valid buffer
    Buffer(void* p, std::size_t sz, std::uint16_t idx) noexcept 
        : ptr(p), size(sz), slot_index(idx) {}
    
    /// Check if buffer is valid
    explicit operator bool() const noexcept { return ptr != nullptr; }
};

/// Allocator that manages buffers using multiple BlockStrategies
/// @tparam NUM_SLOTS Number of strategy slots
template <std::size_t NUM_SLOTS>
class BufferAllocator {
public:
    static constexpr std::size_t kNum = NUM_SLOTS;
    using StrategyArray = std::array<IBlockStrategy*, kNum>;
    
    /// Constructor taking an array of block strategies
    /// @param strategies Array of IBlockStrategy pointers (must outlive this allocator)
    /// Strategies will be automatically sorted by size (ascending) for efficiency
    explicit BufferAllocator(const StrategyArray& strategies) noexcept
        : strategies_(strategies) {
        static_assert(kNum > 0, "BufferAllocator requires at least one slot");
        
        // Sort strategies by size in ascending order
        // Skip null strategies during sort
        std::sort(strategies_.begin(), strategies_.end(), 
            [](IBlockStrategy* a, IBlockStrategy* b) {
                if (!a) return false;  // Nulls go to the end
                if (!b) return true;   // Nulls go to the end
                return a->size() < b->size();
            });
    }
    
    /// Allocate a buffer from the smallest strategy with size >= requested size
    /// @param req_size Minimum size needed
    /// @return Buffer handle, or invalid buffer on failure
    Buffer allocate(std::size_t req_size) noexcept {
        const std::size_t idx = choose_slot(req_size);
        if (idx == npos) return {};  // No suitable strategy found
        
        IBlockStrategy* strategy = strategies_[idx];
        void* p = strategy->allocate();
        if (!p) return {};  // Allocation failed
        
        return Buffer{p, strategy->size(), static_cast<std::uint16_t>(idx)};
    }
    
    /// Deallocate a buffer using its embedded slot index
    /// @param b Buffer to deallocate
    void deallocate(Buffer b) noexcept {
        if (!b.ptr) return;
        
        const std::size_t si = b.slot_index;
        if (si >= kNum) return;  // Sanity check
        
        IBlockStrategy* strategy = strategies_[si];
        if (strategy) {
            strategy->deallocate(b.ptr);
        }
    }
    
    /// Get the number of slots
    static constexpr std::size_t num_slots() noexcept { return kNum; }
    
    /// Get the size of a specific slot's strategy
    std::size_t slot_size(std::size_t idx) const noexcept {
        if (idx < kNum && strategies_[idx]) {
            return strategies_[idx]->size();
        }
        return 0;
    }
    
private:
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);
    
    /// Find the smallest strategy that can hold the requested size
    /// O(k) linear scan where k = number of slots (typically small)
    /// Strategies are automatically sorted by size in constructor
    std::size_t choose_slot(std::size_t n) const noexcept {
        for (std::size_t i = 0; i < kNum; ++i) {
            if (strategies_[i] && strategies_[i]->size() >= n) {
                return i;
            }
        }
        return npos;
    }
    
    StrategyArray strategies_{};
};

}  // namespace ftl::allocator