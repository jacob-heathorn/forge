#pragma once

#include <cstddef>
#include <cstdint>

#include "etl/string.h"
#include "ftl/data_frame.hpp"

namespace ftl::ipv4::udp {

// Provides an interface to an ethernet data frame to access UDP payload data. 
class Payload : private DataFrame {
public:
  // Offsets into the raw frame
  //
  // NOTE: We have not implemented lower frame layers (e.g. EthernetFrame)
  static constexpr std::size_t kPayloadOffset = 0;

  // Construct with payload size
  explicit Payload(std::size_t size)
      : DataFrame(size)
  {}
  Payload() = default;

  using DataFrame::size;
  using DataFrame::operator bool;

  // Access the UDP payload pointer
  uint8_t* data() noexcept { return front() + kPayloadOffset; }
  const uint8_t* data() const noexcept { return front() + kPayloadOffset; }

  // Returns the payload interpreted as characters in an etl::string_view
  etl::string_view string_view() const noexcept {
    const char* data = reinterpret_cast<const char*>(this->data());
    return etl::string_view{ data, this->size() };
  }
};

static_assert(sizeof(Payload) == sizeof(DataFrame));

}
