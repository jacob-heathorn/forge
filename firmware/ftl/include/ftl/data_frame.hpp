#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "ftl/buffer.hpp"
#include "ftl/buffer_bump_pool.hpp"

namespace ftl {

// Custom deleter for Buffer that returns it to a BufferBumpPool
struct BufferReleaser {
    BufferBumpPool* pool;

    void operator()(Buffer* buf) const noexcept {
        if (pool && buf) pool->release(buf);
    }
};

// DataFrame owns a Buffer allocated from a BufferBumpPool
class DataFrame {
public:
    explicit DataFrame(std::size_t size)
    {
      assert(pool() && "DataFrame::initialize() has not been called.");
      // Acquire the buffer.
      buffer_ = std::unique_ptr<Buffer, BufferReleaser>(
          pool()->acquire(size),
          BufferReleaser{pool()}
      );
    }

    // Default constructs an empty data frame.
    DataFrame() = default;

    // Returns true if the buffer is not empty (DatFrame is not empty).
    explicit operator bool() const noexcept {
        return static_cast<bool>(buffer_);
    }

    // Releases (resets) our buffer; afterwards operator bool() will be false
    void reset() noexcept {
      buffer_.reset();
    }

    // Movable but not copyable
    DataFrame(DataFrame&&) noexcept = default;
    DataFrame& operator=(DataFrame&&) noexcept = default;
    DataFrame(const DataFrame&) = delete;
    DataFrame& operator=(const DataFrame&) = delete;
    ~DataFrame() = default;

    // Accessors
    std::size_t       size() const noexcept   { return buffer_->size(); }
    uint8_t*          front() noexcept        { return buffer_->front(); }
    const uint8_t*    front() const noexcept  { return buffer_->front(); }
    uint8_t*          back() noexcept         { return buffer_->back(); }
    const uint8_t*    back() const noexcept   { return buffer_->back(); }
    
    /// Write a trivially-copyable T at offset (must fit)
    template <typename T>
    void set(std::size_t offset, const T& value) { buffer_->set<T>(offset, value); }

    /// Read a trivially-copyable T from offset
    template <typename T>
    T get(std::size_t offset) const { return buffer_->get<T>(offset); }

    static void initialize(ftl::BumpAllocator &allocator)
    {
      static ftl::BufferBumpPool p{allocator};
      pool() = &p;
    }

    static BufferBumpPool*& pool()
    {
      static BufferBumpPool* ptr = nullptr;
      return ptr;
    }

private:
    std::unique_ptr<Buffer, BufferReleaser> buffer_;
};

static_assert(sizeof(DataFrame) == 2 * sizeof(uintptr_t),
              "Expect DataFrame is exactly two machine words in size");

} // namespace ftl
