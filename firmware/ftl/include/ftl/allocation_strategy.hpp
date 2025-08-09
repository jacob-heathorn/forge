#pragma once

#include <cstddef>

namespace ftl {

/// Abstract base class for allocation strategies
/// Provides a common interface for different memory allocation approaches
/// @tparam T Type of objects to allocate/deallocate
template<typename T>
class AllocationStrategy {
public:
    virtual ~AllocationStrategy() = default;

    /// Allocate raw memory for an object of type T
    /// The memory is allocated but the object is NOT constructed
    /// @return Pointer to allocated memory, or nullptr if allocation fails
    virtual T* allocate() = 0;

    /// Deallocate raw memory for an object of type T
    /// The object should already be destructed before calling this
    /// @param ptr Pointer to memory to deallocate
    virtual void deallocate(T* ptr) noexcept = 0;
};

}  // namespace ftl
