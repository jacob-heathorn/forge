#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>


namespace ftl {

/// A non-owning handle to a raw memory block
class Buffer {
public:
  Buffer() noexcept : data_{nullptr}, size_{0} {}
  Buffer(uint8_t* ptr, std::size_t sz) noexcept
    : data_{ptr}, size_{sz} {}

  /// Pointer to first byte
  uint8_t* front() noexcept { return data_; }
  const uint8_t* front() const noexcept { return data_; }

  /// Pointer one-past-last
  uint8_t* back() noexcept { return data_ + size_; }
  const uint8_t* back() const noexcept { return data_ + size_; }

  /// Size in bytes
  std::size_t size() const noexcept { return size_; }

  /// Write a trivially-copyable T at offset (must fit)
  template <typename T>
  void set(std::size_t offset, const T& value) {
    assert(offset + sizeof(T) <= size_);
    *reinterpret_cast<T*>(data_ + offset) = value;
  }

  /// Read a trivially-copyable T from offset
  template <typename T>
  T get(std::size_t offset) const {
    assert(offset + sizeof(T) <= size_);
    return *reinterpret_cast<const T*>(data_ + offset);
  }

private:
  uint8_t*       data_;
  std::size_t    size_;
};

}
