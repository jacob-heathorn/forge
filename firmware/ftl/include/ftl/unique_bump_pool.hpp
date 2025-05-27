#pragma once

#include "ftl/bump_pool.hpp"
#include "ftl/deleter.hpp"


namespace ftl {

//-------------------------------------------------------------------------------------------------
// UniqueBumpPool: combines a bump allocator pool with a polymorphic deleter. Inherits BumpPool<D>
// for pool management and PolymorphicDeleter<B> for runtime-deletions. Provides acquire() to wrap
// new objects in a std::unique_ptr<B, DelegatingDeleter<B>> that returns memory to the pool.
//
// Let base class (B) equal derived class (D) if you don't need to use the polymorphism this class
// provides.
//-------------------------------------------------------------------------------------------------
template <typename D, typename B = D>
class UniqueBumpPool
  : public BumpPool<D>
  , public PolymorphicDeleter<B>
{
public:
  // Inherit all constructors from the base bump pool.
  using BumpPool<D>::BumpPool;

  // Prevent copying.
  UniqueBumpPool(const UniqueBumpPool&) = delete;
  UniqueBumpPool& operator=(const UniqueBumpPool&) = delete;

  // Prevent moving.
  UniqueBumpPool(UniqueBumpPool&&) = delete;
  UniqueBumpPool& operator=(UniqueBumpPool&&) = delete;

  // Override polymorphic deleter call: release object back to the pool.
  // @param ptr Pointer to the object to release.
  void operator()(B* ptr) override
  {
    BumpPool<D>::release(static_cast<D*>(ptr));
  }

  // Acquire an object from the pool and wrap it in a unique_ptr.
  // @tparam Args Constructor argument types.
  // @param args Arguments forwarded to the object's constructor.
  // @return std::unique_ptr<B, DelegatingDeleter<B>> that returns memory on destruction.
  template <typename... Args>
  std::unique_ptr<B, DelegatingDeleter<B>> acquire(Args&&... args)
  {
    return {
      BumpPool<D>::acquire(std::forward<Args>(args)...),
      DelegatingDeleter<B>{this}
    };
  }
};

}
