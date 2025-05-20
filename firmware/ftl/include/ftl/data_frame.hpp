#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>

#include "ftl/deleter.hpp"          // DelegatingDeleter
#include "ftl/unique_bump_pool.hpp" // UniqueBumpPool, BumpAllocator

//-----------------------------------------------------------------------------
// @file    data_frame.hpp
// @brief   Defines an abstract DataBuffer interface and a DataFrame wrapper
//          that allocates fixed-size buffers from a bump pool.
//
// DataBuffer      : interface for buffer size and data access.
// DataBufferN<N>  : concrete N-byte buffer for DataBuffer.
// DataFrame       : non-template wrapper owning DataBuffer via unique_ptr
//                   and DelegatingDeleter, with a Create<N>() factory
//                   method that uses a static UniqueBumpPool.
//-----------------------------------------------------------------------------

// Abstract base for a data buffer of unknown size.
class DataBuffer
{
public:
  virtual ~DataBuffer() = default;

  // Return the buffer’s size in bytes.
  virtual std::size_t size() const = 0;

  // Return a pointer to the first byte of the buffer.
  virtual std::uint8_t* front() = 0;
};

// Fixed-size buffer implementing DataBuffer.
// @tparam N  Number of bytes in the buffer.
template <std::size_t N>
class DataBufferN : public DataBuffer
{
public:
  // Return the compile-time size N.
  std::size_t size() const override { return N; }

  // Return pointer to the buffer’s storage.
  std::uint8_t* front() override { return buffer_; }

private:
  std::uint8_t buffer_[N];
};

// Wrapper that owns a DataBuffer via unique_ptr and a DelegatingDeleter.
class DataFrame
{
public:
  // Factory: allocate an N-byte DataBufferN<N> from a static bump pool.
  // @tparam N          Buffer size in bytes.
  // @param allocator   BumpAllocator used to back all pools.
  // @return            DataFrame owning a unique_ptr to DataBuffer.
  template <std::size_t N>
  static DataFrame Create(ftl::BumpAllocator& allocator)
  {
    // Static pool per-size to minimize allocations.
    static UniqueBumpPool<DataBufferN<N>, DataBuffer> pool{allocator};
    auto buf = pool.acquire();
    return DataFrame(std::move(buf));
  }

  // Movable but not copyable.
  DataFrame(DataFrame&&) noexcept = default;
  DataFrame& operator=(DataFrame&&) noexcept = default;
  DataFrame(const DataFrame&) = delete;
  DataFrame& operator=(const DataFrame&) = delete;

  ~DataFrame() = default;

  // Forwarded accessors.
  std::size_t size() const { return buffer_->size(); }
  std::uint8_t* front() { return buffer_->front(); }

private:
  // Private ctor from the unique_ptr returned by the pool.
  explicit DataFrame(
      std::unique_ptr<DataBuffer, DelegatingDeleter<DataBuffer>> buf)
    : buffer_(std::move(buf))
  {}

  // Owned buffer pointer with custom deleter.
  std::unique_ptr<DataBuffer, DelegatingDeleter<DataBuffer>> buffer_;
};
