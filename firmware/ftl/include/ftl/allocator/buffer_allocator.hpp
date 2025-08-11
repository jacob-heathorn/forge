#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "strategy.hpp"

namespace ftl::allocator {

/// A buffer handle that includes size class information for efficient deallocation
struct Buffer {
    void* ptr;                    // Pointer to allocated memory
    std::size_t size;             // Size of the allocated block
    std::uint16_t class_index;    // Index of the size class used
    
    /// Default constructor creates an invalid buffer
    Buffer() noexcept : ptr(nullptr), size(0), class_index(0) {}
    
    /// Construct a valid buffer
    Buffer(void* p, std::size_t sz, std::uint16_t idx) noexcept 
        : ptr(p), size(sz), class_index(idx) {}
    
    /// Check if buffer is valid
    explicit operator bool() const noexcept { return ptr != nullptr; }
};

/// Allocator that manages buffers of fixed size classes using BlockStrategies
/// @tparam CLASSES Compile-time list of buffer sizes (must be in ascending order)
template <std::size_t... CLASSES>
class BufferAllocator {
public:
    static constexpr std::size_t kNum = sizeof...(CLASSES);
    static constexpr std::array<std::size_t, kNum> kSizes{CLASSES...};
    
    using StrategyArray = std::array<IBlockStrategy*, kNum>;
    
    /// Constructor taking an array of block strategies
    /// @param strategies Array of IBlockStrategy pointers (must outlive this allocator)
    explicit BufferAllocator(const StrategyArray& strategies) noexcept
        : strategies_(strategies) {
        static_assert(kNum > 0, "BufferAllocator requires at least one size class");
        // Verify sizes are in ascending order at compile time
        if constexpr (kNum > 1) {
            constexpr bool sorted = []() {
                for (std::size_t i = 1; i < kNum; ++i) {
                    if (kSizes[i] <= kSizes[i-1]) return false;
                }
                return true;
            }();
            static_assert(sorted, "Buffer size classes must be in ascending order");
        }
    }
    
    /// Allocate a buffer of the smallest size class >= requested size
    /// @param req_size Minimum size needed
    /// @return Buffer handle, or invalid buffer on failure
    Buffer allocate(std::size_t req_size) noexcept {
        const std::size_t idx = choose_class(req_size);
        if (idx == npos) return {};  // Requested size too large
        
        IBlockStrategy* strategy = strategies_[idx];
        if (!strategy) return {};  // No strategy for this class
        
        void* p = strategy->allocate();
        if (!p) return {};  // Allocation failed
        
        return Buffer{p, kSizes[idx], static_cast<std::uint16_t>(idx)};
    }
    
    /// Deallocate a buffer using its embedded class index
    /// @param b Buffer to deallocate
    void deallocate(Buffer b) noexcept {
        if (!b.ptr) return;
        
        const std::size_t ci = b.class_index;
        if (ci >= kNum) return;  // Sanity check
        
        IBlockStrategy* strategy = strategies_[ci];
        if (strategy) {
            strategy->deallocate(b.ptr);
        }
    }
    
    /// Get the number of size classes
    static constexpr std::size_t num_classes() noexcept { return kNum; }
    
    /// Get the size of a specific class
    static constexpr std::size_t class_size(std::size_t idx) noexcept {
        return (idx < kNum) ? kSizes[idx] : 0;
    }
    
private:
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);
    
    /// Find the smallest size class that can hold the requested size
    /// O(k) linear scan where k = number of classes (typically small)
    /// If k grows large, consider switching to binary search
    static constexpr std::size_t choose_class(std::size_t n) noexcept {
        for (std::size_t i = 0; i < kNum; ++i) {
            if (n <= kSizes[i]) return i;
        }
        return npos;
    }
    
    StrategyArray strategies_{};
};

// Common buffer size configurations
using BufferAllocator64_128_256 = BufferAllocator<64, 128, 256>;
using BufferAllocator256_512_1024_2048 = BufferAllocator<256, 512, 1024, 2048>;
using BufferAllocatorPowerOfTwo = BufferAllocator<64, 128, 256, 512, 1024, 2048, 4096>;

}  // namespace ftl::allocator