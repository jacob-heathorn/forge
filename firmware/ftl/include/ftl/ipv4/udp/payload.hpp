#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <cassert>
#include <type_traits>

#include "etl/string.h"
#include "ftl/buffer.hpp"
#include "ftl/allocator/buffer_allocator.hpp"
#include "ftl/allocator/strategy.hpp"

namespace ftl::ipv4::udp {

// Custom deleter for Buffer that returns it to the allocator
struct PayloadBufferDeleter {
    allocator::BufferAllocator* allocator;

    void operator()(Buffer* buf) const noexcept {
        if (allocator && buf) allocator->deallocate(buf);
    }
};

// Provides an interface to an ethernet data frame to access UDP payload data. 
class Payload {
private:
  // Offsets into the raw frame
  //
  // NOTE: We have not implemented lower frame layers (e.g. EthernetFrame)
  static constexpr std::size_t kPayloadOffset = 0;

  std::unique_ptr<Buffer, PayloadBufferDeleter> buffer_;
  std::size_t actual_size_ = 0;  // Actual payload size (may be less than buffer size)

  static allocator::BufferAllocator*& allocator() {
    static allocator::BufferAllocator* ptr = nullptr;
    return ptr;
  }

public:
  // Initialize with a buffer strategy
  static void initialize(allocator::IBufferStrategy& strategy) {
    static allocator::BufferAllocator alloc{strategy};
    allocator() = &alloc;
  }

  // Construct with payload size
  explicit Payload(std::size_t size) {
    assert(allocator() && "Payload::initialize() has not been called.");
    buffer_ = std::unique_ptr<Buffer, PayloadBufferDeleter>(
        allocator()->allocate(size),
        PayloadBufferDeleter{allocator()}
    );
  }
  
  Payload() = default;

  // Movable but not copyable
  Payload(Payload&&) noexcept = default;
  Payload& operator=(Payload&&) noexcept = default;
  Payload(const Payload&) = delete;
  Payload& operator=(const Payload&) = delete;
  ~Payload() = default;

  // Check if buffer is valid
  explicit operator bool() const noexcept {
    return static_cast<bool>(buffer_);
  }

  // Access the UDP payload pointer
  uint8_t* front() noexcept { return buffer_->front() + kPayloadOffset; }
  const uint8_t* front() const noexcept { return buffer_->front() + kPayloadOffset; }
  std::size_t size() const noexcept { return buffer_->size() - kPayloadOffset; }

  // Returns the payload interpreted as characters in an etl::string_view
  etl::string_view string_view() const noexcept {
    const char* data = reinterpret_cast<const char*>(this->front());
    return etl::string_view{ data, this->size() };
  }
};

// Payload contains a unique_ptr with custom deleter, which is 2 pointers

}
