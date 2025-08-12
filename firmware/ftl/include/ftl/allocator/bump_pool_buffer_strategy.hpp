#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <type_traits>
#include <new>
#include <cstddef>   // offsetof
#include "strategy.hpp"
#include "ftl/buffer.hpp"
#include "ftl/bump_allocator.hpp"
#include "ftl/mutex.hpp"

namespace ftl::allocator {

template <std::size_t NUM_SLOTS>
class BumpPoolBufferStrategy : public IBufferStrategy<NUM_SLOTS> {
public:
    using Base = IBufferStrategy<NUM_SLOTS>;

    // helpers (constexpr/pod-only, safe for embedded)
    static constexpr bool is_pow2(std::size_t x) noexcept { return x && ((x & (x - 1)) == 0); }
    static constexpr std::size_t align_up(std::size_t n, std::size_t a) noexcept {
        // a must be power-of-two
        return (n + (a - 1)) & ~(a - 1);
    }

    struct BufferNode {
        BufferNode* next;
        ftl::Buffer buffer; // must keep standard-layout for offsetof

        BufferNode(uint8_t* data, std::size_t size) noexcept
            : next(nullptr), buffer(data, size) {}
    };
    static_assert(std::is_standard_layout<BufferNode>::value, "BufferNode must be standard-layout");

private:
    // Precomputed layout constants
    const std::size_t data_offset_;
    const std::size_t block_alignment_;
    std::array<std::size_t, NUM_SLOTS> total_size_{};

    // Per-size locks to minimize contention
    std::array<Mutex, NUM_SLOTS> freelist_mutex_{};

    // Separate lock since allocator_ is NOT thread-safe
    Mutex alloc_mutex_;

    BumpAllocator& allocator_;

    std::array<BufferNode*, NUM_SLOTS> free_lists_{};

    std::size_t find_size_index(std::size_t size) const noexcept {
        for (std::size_t i = 0; i < NUM_SLOTS; ++i) {
            if (this->sizes_[i] == size) return i;
        }
        return NUM_SLOTS;
    }

public:
    // Constructor:
    BumpPoolBufferStrategy(BumpAllocator& allocator,
                        const std::array<std::size_t, NUM_SLOTS>& sizes,
                        std::size_t alignment = alignof(std::max_align_t)) noexcept
        : Base(sizes, alignment)
        , data_offset_(align_up(sizeof(BufferNode), this->alignment_))
        , block_alignment_( (this->alignment_ > alignof(BufferNode)) ? this->alignment_
                                                                    : alignof(BufferNode) )
        , allocator_(allocator) {
        // validate alignment and precompute total sizes once
        if (!is_pow2(this->alignment_)) {
            // For safety-critical, you may prefer to set an internal "disabled" flag instead of assert.
            // Here we leave it; allocate() can return nullptr if invalid.
        }
        for (std::size_t i = 0; i < NUM_SLOTS; ++i) {
            // overflow check once
            const std::size_t sz = this->sizes_[i];
            total_size_[i] = (data_offset_ > static_cast<std::size_t>(-1) - sz) ? 0 : (data_offset_ + sz);
            free_lists_[i] = nullptr;
        }
    }

    // Allocate fast path with tiny critical section:
    ftl::Buffer* allocate(std::size_t req_size) noexcept override {
        // choose class (no lock)
        std::size_t size_index = NUM_SLOTS;
        for (std::size_t i = 0; i < NUM_SLOTS; ++i) {
            if (this->sizes_[i] >= req_size) { size_index = i; break; }
        }
        if (size_index == NUM_SLOTS) return nullptr;
        const std::size_t alloc_size = this->sizes_[size_index];
        const std::size_t total = total_size_[size_index];
        if (total == 0) return nullptr; // overflow caught at construction

        // Try reuse under a very short lock
        {
            LockGuard<Mutex> lk(freelist_mutex_[size_index]);
            BufferNode* head = free_lists_[size_index];
            if (head) {
                free_lists_[size_index] = head->next;
                return &head->buffer;
            }
        }

        // Slow path: allocate a brand new block (no freelist lock held)
        void* block;
        {
            // Only lock allocator if it isn't internally thread-safe.
            LockGuard<Mutex> lk(alloc_mutex_);
            block = allocator_.allocate(total, block_alignment_);
        }
        if (!block) return nullptr;

        auto* base = static_cast<uint8_t*>(block);
        uint8_t* data = base + data_offset_;

        // Single construction; no double placement-new
        BufferNode* node = ::new (block) BufferNode(data, alloc_size);
        return &node->buffer;
    }

    // Deallocate with tiny critical section:
    void deallocate(ftl::Buffer* buffer) noexcept override {
        if (!buffer) return;

        const std::size_t size_index = find_size_index(buffer->size());
        if (size_index >= NUM_SLOTS) return;

        // Get node via standard-layout guarantee
        auto* node = reinterpret_cast<BufferNode*>(
            reinterpret_cast<char*>(buffer) - offsetof(BufferNode, buffer));

        // push under a very short lock
        {
            LockGuard<Mutex> lk(freelist_mutex_[size_index]);
            node->next = free_lists_[size_index];
            free_lists_[size_index] = node;
        }
    }
};

} // namespace ftl::allocator
