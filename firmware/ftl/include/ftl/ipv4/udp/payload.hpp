#pragma once

#include <cstddef>
#include <cstdint>

#include "etl/string.h"
#include "ftl/data_frame.hpp"

namespace ftl::ipv4::udp {

// Provides an interface to an ethernet data frame to access UDP payload data. 
class Payload : private DataFrame {
private:
  // Offsets into the raw frame
  //
  // NOTE: We have not implemented lower frame layers (e.g. EthernetFrame)
  static constexpr std::size_t kPayloadOffset = 0;

public:
  // Construct with payload size
  explicit Payload(std::size_t size)
      : DataFrame(size)
  {}
  Payload() = default;

  using DataFrame::operator bool;

  // Access the UDP payload pointer - overrides DataFrame::front() with offset
  uint8_t* front() noexcept { return DataFrame::front() + kPayloadOffset; }
  const uint8_t* front() const noexcept { return DataFrame::front() + kPayloadOffset; }
  std::size_t size() const noexcept { return DataFrame::size() - kPayloadOffset; }

  // Returns the payload interpreted as characters in an etl::string_view
  etl::string_view string_view() const noexcept {
    const char* data = reinterpret_cast<const char*>(this->front());
    return etl::string_view{ data, this->size() };
  }
};

static_assert(sizeof(Payload) == sizeof(DataFrame));

}
