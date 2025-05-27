#pragma once

#include "ftl/ipv4/address.hpp"


namespace ftl::ipv4 {

class Mask : public Address {
  public:
    Mask (uint8_t m1, uint8_t m2, uint8_t m3, uint8_t m4)
      : Address(m1, m2, m3, m4)
    {
      assert(this->IsValid());
    }
  
    // Checks that the mask is contiguous ones then zeros
    constexpr bool IsValid() const noexcept {
      uint32_t m   = ToUint32();
      uint32_t inv = ~m;                   // host‐portion mask: 00000000.00000000.00000000.11111111
      // valid if inv is 0 (i.e. /32) or a contiguous run of ones:
      // inv = 0b000...0111...1  ⇒  inv & (inv + 1) == 0
      return inv == 0u || ((inv & (inv + 1u)) == 0u);
    }
};

}
