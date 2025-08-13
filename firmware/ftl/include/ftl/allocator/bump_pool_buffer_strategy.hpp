#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <type_traits>
#include <new>
#include <cstddef>   // offsetof
#include "strategy.hpp"
#include "ftl/buffer.hpp"
#include "ftl/allocator/bump_allocator.hpp"
#include "ftl/mutex.hpp"

namespace ftl::allocator {

class BumpPoolBufferStrategy : public IBufferStrategy {
public:
    using Base = IBufferStrategy;

    struct BufferNode {
        BufferNode* next;
        ftl::Buffer buffer; // must keep standard-layout for offsetof
        std::size_t slot_index; // Which slot/free list this belongs to

        BufferNode(uint8_t* data, std::size_t requested_size, std::size_t slot_idx) noexcept
            : next(nullptr), buffer(data, requested_size), slot_index(slot_idx) {}
    };
    static_assert(std::is_standard_layout<BufferNode>::value, "BufferNode must be standard-layout");

private:
    // Per-size locks to minimize contention
    std::array<Mutex, kMaxSlots> freelist_mutex_{};

    // Separate lock since allocator_ is NOT thread-safe
    Mutex alloc_mutex_;

    BumpAllocator& allocator_;

    std::array<BufferNode*, kMaxSlots> free_lists_{};

public:
    // Constructor:
    template <std::size_t N>
    BumpPoolBufferStrategy(BumpAllocator& allocator,
                        const std::array<std::size_t, N>& sizes,
                        std::size_t alignment = alignof(std::max_align_t)) noexcept
        : Base(sizes, alignment)
        , allocator_(allocator) {
        // Initialize free lists to nullptr
        for (auto& list : free_lists_) {
            list = nullptr;
        }
    }

    // Allocate fast path with tiny critical section:
    ftl::Buffer* allocate(std::size_t req_size) noexcept override {
        // choose class (no lock)
        std::size_t size_index = num_slots();
        for (std::size_t i = 0; i < num_slots(); ++i) {
            if (this->size(i) >= req_size) { size_index = i; break; }
        }
        if (size_index == num_slots()) return nullptr;
        const std::size_t alloc_size = this->size(size_index);

        // Try reuse under a very short lock
        {
            LockGuard<Mutex> lk(freelist_mutex_[size_index]);
            BufferNode* head = free_lists_[size_index];
            if (head) {
                free_lists_[size_index] = head->next;
                head->buffer = ftl::Buffer(head->buffer.front(), req_size);  // Update size
                return &head->buffer;
            }
        }

        // Slow path: allocate BufferNode and data separately
        BufferNode* node;
        void* data;
        {
            // Only lock allocator if it isn't internally thread-safe.
            LockGuard<Mutex> lk(alloc_mutex_);
            // Allocate node
            node = static_cast<BufferNode*>(allocator_.allocate(sizeof(BufferNode), alignof(BufferNode)));
            if (!node) return nullptr;
            
            // Allocate data with requested alignment
            data = allocator_.allocate(alloc_size, this->alignment_);
            if (!data) {
                // Can't deallocate node back to bump allocator, just leave it
                return nullptr;
            }
        }

        // Construct BufferNode with requested size and slot index
        new (node) BufferNode(static_cast<uint8_t*>(data), req_size, size_index);
        return &node->buffer;
    }

    // Deallocate with tiny critical section:
    void deallocate(ftl::Buffer* buffer) noexcept override {
        if (!buffer) return;

        // Get node via standard-layout guarantee
        auto* node = reinterpret_cast<BufferNode*>(
            reinterpret_cast<char*>(buffer) - offsetof(BufferNode, buffer));

        // Use the stored slot_index to return to the correct free list
        const std::size_t size_index = node->slot_index;
        if (size_index >= num_slots()) return;

        // push under a very short lock
        {
            LockGuard<Mutex> lk(freelist_mutex_[size_index]);
            node->next = free_lists_[size_index];
            free_lists_[size_index] = node;
        }
    }
};

} // namespace ftl::allocator
