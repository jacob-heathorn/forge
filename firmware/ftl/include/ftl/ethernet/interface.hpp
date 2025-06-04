#pragma once

#include <memory>

#include "ftl/ipv4/endpoint.hpp"
#include "ftl/ipv4/mask.hpp"
#include "ftl/ipv4/udp/socket.hpp"


namespace ftl::ethernet {

class Interface
{
  public:

    Interface(ftl::ipv4::Address address, ftl::ipv4::Mask mask)
      : address_{address}, mask_{mask} {}
    virtual ~Interface() = default;

    // No copying or moving.
    Interface(const Interface&) = delete;            // Delete copy constructor
    Interface& operator=(const Interface&) = delete; // Delete copy assignment operator
    Interface(Interface&&) = delete;                 // Delete move constructor
    Interface& operator=(Interface&&) = delete;      // Delete move assignment operator

    virtual ftl::ipv4::udp::SocketPtr CreateUdpSocket() = 0;

    ftl::ipv4::Address address() const noexcept { return address_; }
    ftl::ipv4::Mask mask() const noexcept { return mask_; }
  
  private:
    ftl::ipv4::Address address_;
    ftl::ipv4::Mask mask_;
};

}
