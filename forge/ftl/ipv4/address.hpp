#pragma once

#include <cstdint>

#include "etl/array.h"
#include "etl/string.h"
#include "etl/string_stream.h"


namespace ftl::ipv4 {

class Address {
public:
  explicit constexpr Address(uint8_t o1, uint8_t o2, uint8_t o3, uint8_t o4)
    : octets_{ o1, o2, o3, o4 }
  {}

  Address(const char* s) : Address(etl::string_view{s}) {}
  Address(etl::string_view sv)
  {
    etl::array<uint8_t,4> tmp{};
    size_t idx = 0;
    int    val = 0;

    for (char c : sv) {
      if (c >= '0' && c <= '9') {
        val = val * 10 + (c - '0');
        assert(val <= 255);
      }
      else if (c == '.' && idx < 4) {
        tmp[idx++] = static_cast<uint8_t>(val);
        val = 0;
      }
      else {
        assert(false && "Invalid character in IPv4 string");
      }
    }
    // must have seen exactly 3 dots
    assert(idx == 3);
    tmp[3] = static_cast<uint8_t>(val);

    octets_ = tmp;
  }

  explicit constexpr Address(uint32_t addr_network_order) noexcept
    : octets_ {
        // top byte is octets_[0], then shift down
        static_cast<uint8_t>(addr_network_order >> 24),
        static_cast<uint8_t>(addr_network_order >> 16),
        static_cast<uint8_t>(addr_network_order >>  8),
        static_cast<uint8_t>(addr_network_order      )
      }
  {}


  constexpr etl::array<uint8_t,4> Octets() const noexcept { return octets_; }

  // Converts to 32-bit network-order integer
  constexpr uint32_t ToUint32() const noexcept {
    return (uint32_t(octets_[0]) << 24)
         | (uint32_t(octets_[1]) << 16)
         | (uint32_t(octets_[2]) <<  8)
         | (uint32_t(octets_[3]) <<  0);
  }

  // Returns octets as a string.
  etl::string<16> ToString() const {
    etl::string<16> s;
    etl::string_stream ss(s);
    ss << int(octets_[0]) << "."
       << int(octets_[1]) << "."
       << int(octets_[2]) << "."
       << int(octets_[3]);
    return s;
  }

  // Equality operators
  bool operator==(Address const& o) const noexcept {
    return octets_ == o.octets_;
  }
  bool operator!=(Address const& o) const noexcept {
    return !(*this == o);
  }
private:
  etl::array<uint8_t,4> octets_;
};

}
