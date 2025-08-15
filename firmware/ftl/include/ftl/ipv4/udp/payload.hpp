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

namespace ftl::ipv4::udp {

// Provides an interface to an ethernet data frame to access UDP payload data. 
class Payload {
private:
  // Static deleter for Payload buffers
  class Deleter : public ftl::IDeleter {
  public:
    inline static allocator::BufferAllocator* allocator = nullptr;
    
    void operator()(void* ptr) override {
      if (ptr && allocator) {
        allocator->deallocate(static_cast<Buffer*>(ptr));
      }
    }
  };
  
  // Offsets into the raw frame
  //
  // NOTE: We have not implemented lower frame layers (e.g. EthernetFrame)
  static constexpr std::size_t kPayloadOffset = 0;

  inline static Deleter deleter_;
  ftl::unique_ptr<Buffer> buffer_;  

public:
  // Initialize with a buffer allocator
  static void initialize(allocator::BufferAllocator& alloc) {
    Deleter::allocator = &alloc;
  }

  // Construct with payload size
  explicit Payload(std::size_t size) {
    assert(Deleter::allocator && "Payload::initialize() has not been called.");
    Buffer* buf = Deleter::allocator->allocate(size);
    if (buf) {
      buffer_ = ftl::unique_ptr<Buffer>(buf, &deleter_);
    }
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

// Payload contains a unique_ptr with deleter pointer, which is 2 pointers

}
