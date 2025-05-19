#pragma once

#include "ftl/bump_pool.hpp"

template <typename D, typename B = D>
class UniqueBumpPool
  : public BumpPool<D>              // get all the pool logic “for free”
  , public PolymorphicDeleter<B>    // satisfy the polymorphic-deleter interface
{
public:
  // Inherit the pool’s constructor(s):
  using BumpPool<D>::BumpPool;

  // We still want no copies or moves:
  UniqueBumpPool(const UniqueBumpPool&)            = delete;
  UniqueBumpPool& operator=(const UniqueBumpPool&) = delete;
  UniqueBumpPool(UniqueBumpPool&&)                 = delete;
  UniqueBumpPool& operator=(UniqueBumpPool&&)      = delete;

  // PolymorphicDeleter<B> requires us to override operator():
  void operator()(B* ptr) override
  {
    // call the pool’s release, casting back to D*
    BumpPool<D>::release(static_cast<D*>(ptr));
  }

  // Acquire returns a unique_ptr that uses *this* as its deleter.
  template <typename... Args>
  std::unique_ptr<B, DelegatingDeleter<B>> acquire(Args&&... args)
  {
    // Note: unique_ptr will copy/move your deleter (UniqueBumpPool) by value
    return {
      BumpPool<D>::acquire(std::forward<Args>(args)...),
      DelegatingDeleter<B>{this}
    };
  }
};
