#pragma once

#include "ftl/ipv4/address.hpp"


namespace ftl::ipv4 {

class Endpoint {
public:
  Endpoint() = default;

  // 1) Main ctor: Ipv4Address + port
  constexpr Endpoint(Address addr, uint16_t port) noexcept
    : address_(addr), port_(port)
  {}

  // 2) Convenience from string literal or etl::string_view
  Endpoint(const char* addr, uint16_t port)
    : Endpoint(Address{addr}, port)
  {}
  Endpoint(etl::string_view addr, uint16_t port)
    : Endpoint(Address{addr}, port)
  {}

  // Accessors
  constexpr Address address() const noexcept { return address_; }
  constexpr void set_address(const Address address) noexcept { address_ = address; }
  constexpr uint16_t port()    const noexcept { return port_;    }
  constexpr void set_port(const uint16_t port) noexcept { port_ = port; }

  etl::string<21> ToString() const {
    etl::string<21> s;
    etl::string_stream ss(s);
    ss << address().ToString() << ":" << port();
    return s;
  }

private:
  Address address_ = {"0.0.0.0"};
  uint16_t    port_ = 0;
};

}
