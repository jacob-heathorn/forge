#pragma once
#include <cstddef>
#include <cstdint>

// Forward declarations
struct BumpAllocator;
struct Buffer;

// ============================================================================
// Base "interface" for any allocation strategy: function pointers, no virtuals
// ============================================================================
struct AllocationStrategy {
    void* (*allocate)(void* self, std::size_t size) noexcept;
    void  (*deallocate)(void* self, void* ptr) noexcept;
    void* self; // pointer to concrete strategy state
};

// ============================================================================
// Specific strategy types
// ============================================================================

struct MallocAllocationStrategy {
    static AllocationStrategy make() noexcept;
};

struct BumpPoolAllocationStrategy {
    static AllocationStrategy make(std::size_t block_size, BumpAllocator& bump) noexcept;
    // allocate/deallocate implementations will be in .cpp
};

struct FixedAllocationStrategy {
    static AllocationStrategy make(std::size_t block_size, std::size_t max_blocks, BumpAllocator& bump) noexcept;
};

// ============================================================================
// Object allocator (typed) using any AllocationStrategy
// ============================================================================
template <typename T>
struct ObjectAllocator {
    explicit ObjectAllocator(AllocationStrategy strat) noexcept : strat_(strat) {}

    T* allocate() noexcept {
        if (!strat_.allocate) return nullptr;
        void* raw = strat_.allocate(strat_.self, sizeof(T));
        if (!raw) return nullptr;
        return new (raw) T();
    }

    void deallocate(T* p) noexcept {
        if (!p || !strat_.deallocate) return;
        p->~T();
        strat_.deallocate(strat_.self, p);
    }

private:
    AllocationStrategy strat_;
};

// ============================================================================
// Buffer allocator (untyped) with size-class map
// ============================================================================
template <std::size_t... CLASSES>
struct BufferAllocator {
    static constexpr std::size_t kNum = sizeof...(CLASSES);
    static constexpr std::array<std::size_t, kNum> kSizes{CLASSES...};

    // The caller provides one strategy per class, in the same order as CLASSES.
    explicit BufferAllocator(const std::array<AllocationStrategy, kNum>& strategies) noexcept
        : strategies_(strategies) {}

    // Smallest class >= requested size; nullptr if none or if strategy fails.
    Buffer* allocate(std::size_t req_size) noexcept {
        const std::size_t idx = choose_class(req_size);
        if (idx == npos) return nullptr;
        auto& s = strategies_[idx];
        if (!s.allocate) return nullptr;
        return static_cast<Buffer*>(s.allocate(s.self, kSizes[idx]));
    }

    // Caller must pass the same req_size they requested at allocation time.
    // (Or you can store class index inside Buffer and drop req_size — your call.)
    void deallocate(Buffer* buf, std::size_t req_size) noexcept {
        if (!buf) return;
        const std::size_t idx = choose_class(req_size);
        if (idx == npos) return;
        auto& s = strategies_[idx];
        if (s.deallocate) s.deallocate(s.self, buf);
    }

private:
    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    // O(k) linear scan (k is tiny). If CLASSES are sorted, this is deterministic and fast.
    static constexpr std::size_t choose_class(std::size_t n) noexcept {
        for (std::size_t i = 0; i < kNum; ++i) {
            if (n <= kSizes[i]) return i;
        }
        return npos;
    }

    std::array<AllocationStrategy, kNum> strategies_{};
};