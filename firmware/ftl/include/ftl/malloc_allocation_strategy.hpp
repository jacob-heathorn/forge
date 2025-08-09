#pragma once

#include <cstddef>
#include <cstdlib>
#include <new>
#include "ftl/allocation_strategy.hpp"

namespace ftl {

/// Allocation strategy that uses malloc/free for memory management
/// @tparam T Type of objects to allocate
template<typename T>
class MallocAllocationStrategy : public AllocationStrategy<T> {
public:
    MallocAllocationStrategy() = default;
    ~MallocAllocationStrategy() override = default;

    // Non-copyable, non-movable
    MallocAllocationStrategy(const MallocAllocationStrategy&) = delete;
    MallocAllocationStrategy& operator=(const MallocAllocationStrategy&) = delete;
    MallocAllocationStrategy(MallocAllocationStrategy&&) = delete;
    MallocAllocationStrategy& operator=(MallocAllocationStrategy&&) = delete;

    /// Allocate raw memory for an object of type T using malloc
    /// Does NOT construct the object - caller must use placement new
    /// @return Pointer to allocated memory, or nullptr if allocation fails
    T* allocate() override {
        void* mem = std::malloc(sizeof(T));
        if (!mem) {
            return nullptr;  // Allocation failed
        }
        return reinterpret_cast<T*>(mem);
    }

    /// Deallocate raw memory for an object of type T using free
    /// Object should already be destructed before calling this
    /// @param ptr Pointer to memory to deallocate
    void deallocate(T* ptr) noexcept override {
        if (ptr) {
            std::free(ptr);
        }
    }
};

}  // namespace ftl