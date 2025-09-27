#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <type_traits>

#include "etl/string.h"
#include "ftl/buffer.hpp"
#include "ftl/allocator/buffer_allocator.hpp"
#include "ftl/allocator/strategy.hpp"
#include "ftl/memory.hpp"

namespace ftl::allocator {

// DataFrame uses CRTP (Curiously Recurring Template Pattern) where the derived
// class passes itself as the template parameter. This ensures each derived class
// gets its own static allocator instance, preventing different frame types from
// sharing the same allocator. Example: class Payload : public DataFrame<Payload>
template <typename Derived>
class DataFrame {
protected:
  class Deleter : public ftl::IDeleter {
  public:
    inline static BufferAllocator* allocator = nullptr;

    void operator()(void* ptr) override {
      if (ptr && allocator) {
        allocator->deallocate(static_cast<Buffer*>(ptr));
      }
    }
  };

  inline static Deleter deleter_;
  ftl::unique_ptr<Buffer> buffer_;

public:
  static void initialize(BufferAllocator& alloc) {
    Deleter::allocator = &alloc;
  }

  explicit DataFrame(std::size_t size) {
    assert(Deleter::allocator && "DataFrame::initialize() has not been called.");
    Buffer* buf = Deleter::allocator->allocate(size);
    if (buf) {
      buffer_ = ftl::unique_ptr<Buffer>(buf, &deleter_);
    }
  }

  DataFrame() = default;

  // Movable but not copyable
  DataFrame(DataFrame&&) noexcept = default;
  DataFrame& operator=(DataFrame&&) noexcept = default;
  DataFrame(const DataFrame&) = delete;
  DataFrame& operator=(const DataFrame&) = delete;
  virtual ~DataFrame() = default;

  explicit operator bool() const noexcept {
    return static_cast<bool>(buffer_);
  }

  uint8_t* front() noexcept {
    return buffer_ ? buffer_->front() : nullptr;
  }

  const uint8_t* front() const noexcept {
    return buffer_ ? buffer_->front() : nullptr;
  }

  std::size_t size() const noexcept {
    return buffer_ ? buffer_->size() : 0;
  }

  // Returns the buffer data interpreted as characters in an etl::string_view
  etl::string_view string_view() const noexcept {
    const char* data = reinterpret_cast<const char*>(this->front());
    return etl::string_view{ data, this->size() };
  }
};

// DataFrame contains: unique_ptr (2 pointers) + vtable pointer from Deleter inheriting IDeleter
static_assert(sizeof(DataFrame<int>) == 3 * sizeof(void*));

}