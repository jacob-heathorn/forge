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
    BumpAllocator& allocator_;
    mutable Mutex mutex_;

    std::array<BufferNode*, NUM_SLOTS> free_lists_{};

    std::size_t find_size_index(std::size_t size) const noexcept {
        for (std::size_t i = 0; i < NUM_SLOTS; ++i) {
            if (this->sizes_[i] == size) return i;
        }
        return NUM_SLOTS;
    }

public:
    BumpPoolBufferStrategy(BumpAllocator& allocator,
                           const std::array<std::size_t, NUM_SLOTS>& sizes,
                           std::size_t alignment = alignof(std::max_align_t)) noexcept
        : Base(sizes, alignment), allocator_(allocator) {
        // validate alignment once (fail fast in debug; return nullptr on allocate in release)
        // For embedded, you can flip this to harden at runtime if needed.
    }

    ftl::Buffer* allocate(std::size_t req_size) noexcept override {
        // choose size class
        std::size_t alloc_size = 0;
        std::size_t size_index = NUM_SLOTS;
        for (std::size_t i = 0; i < NUM_SLOTS; ++i) {
            if (this->sizes_[i] >= req_size) { alloc_size = this->sizes_[i]; size_index = i; break; }
        }
        if (alloc_size == 0) return nullptr;

        // alignment preconditions
        if (!is_pow2(this->alignment_) || this->alignment_ < alignof(std::max_align_t)) {
            // If you want stricter: allow any power-of-two; the block alignment handles BufferNode anyway.
            if (!is_pow2(this->alignment_)) return nullptr;
        }

        LockGuard<Mutex> lock(mutex_);

        // fast path: reuse
        if (free_lists_[size_index]) {
            BufferNode* node = free_lists_[size_index];
            free_lists_[size_index] = node->next;
            return &node->buffer;
        }

        // layout: [BufferNode][padding]-> aligned data
        const std::size_t data_offset = align_up(sizeof(BufferNode), this->alignment_);

        // overflow check: data_offset + alloc_size
        if (data_offset > static_cast<std::size_t>(-1) - alloc_size) return nullptr;
        const std::size_t total_size = data_offset + alloc_size;

        const std::size_t block_alignment =
            this->alignment_ > alignof(BufferNode) ? this->alignment_ : alignof(BufferNode);

        void* block = allocator_.allocate(total_size, block_alignment);
        if (!block) return nullptr;

        auto* base = static_cast<uint8_t*>(block);
        uint8_t* data = base + data_offset;

        // single construction, no double-placement-new on buffer
        BufferNode* node = ::new (block) BufferNode(data, alloc_size);
        return &node->buffer;
    }

    void deallocate(ftl::Buffer* buffer) noexcept override {
        if (!buffer) return;

        const std::size_t size_index = find_size_index(buffer->size());
        if (size_index >= NUM_SLOTS) return;

        // obtain node from embedded buffer (safe under standard-layout)
        auto* node = reinterpret_cast<BufferNode*>(
            reinterpret_cast<char*>(buffer) - offsetof(BufferNode, buffer));

        LockGuard<Mutex> lock(mutex_);
        node->next = free_lists_[size_index];
        free_lists_[size_index] = node;
    }
};

} // namespace ftl::allocator
