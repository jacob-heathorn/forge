#pragma once

#include <memory>
#include <utility>
#include "ftl/deleter.hpp"
#include "ftl/allocator/obj_allocator.hpp"
#include "ftl/allocator/strategy.hpp"

namespace ftl::allocator {

/// @brief Allocator that wraps objects in unique_ptr with automatic deallocation
/// Combines ObjAllocator with PolymorphicDeleter to provide std::unique_ptr wrapping
/// @tparam D Type to allocate
template<typename D>
class UniqueObjAllocator 
    : public ObjAllocator<D>
    , public PolymorphicDeleter {
public:
    /// Constructor that takes a reference to an IObjStrategy
    /// @param strategy Object allocation strategy to use (must outlive this allocator)
    explicit UniqueObjAllocator(IObjStrategy<D>& strategy)
        : ObjAllocator<D>(strategy)
        , strategy_(strategy) {
    }

    // Prevent copying
    UniqueObjAllocator(const UniqueObjAllocator&) = delete;
    UniqueObjAllocator& operator=(const UniqueObjAllocator&) = delete;

    // Prevent moving
    UniqueObjAllocator(UniqueObjAllocator&&) = delete;
    UniqueObjAllocator& operator=(UniqueObjAllocator&&) = delete;

    /// Override polymorphic deleter call: destruct and deallocate object
    /// @param ptr Pointer to the object to release (as void*)
    void operator()(void* ptr) override {
        if (ptr) {
            // Cast to actual type and use ObjAllocator's deallocate
            ObjAllocator<D>::deallocate(static_cast<D*>(ptr));
        }
    }

    /// Acquire an object from the allocator and wrap it in a unique_ptr
    /// @tparam B Base type for the unique_ptr (D must be derived from B, defaults to D)
    /// @tparam Args Constructor argument types
    /// @param args Arguments forwarded to the object's constructor
    /// @return std::unique_ptr<B, DelegatingDeleter<B>> that deallocates on destruction
    template<typename B = D, typename... Args>
    std::unique_ptr<B, DelegatingDeleter<B>> acquire(Args&&... args) {
        static_assert(std::is_base_of_v<B, D> || std::is_same_v<B, D>, 
                      "D must be derived from B or the same as B");
        D* obj = ObjAllocator<D>::allocate(std::forward<Args>(args)...);
        return std::unique_ptr<B, DelegatingDeleter<B>>{
            obj,
            DelegatingDeleter<B>{this}
        };
    }

    /// Get the underlying strategy (for compatibility/inspection)
    IObjStrategy<D>& strategy() { return strategy_; }
    const IObjStrategy<D>& strategy() const { return strategy_; }

private:
    IObjStrategy<D>& strategy_;
};

}  // namespace ftl::allocator
