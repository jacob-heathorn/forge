// include/ftl/native_ethernet_interface.hpp
#pragma once

#include "ftl/ethernet/interface.hpp"
#include "ftl/ipv4/udp/socket.hpp"

namespace ftl::ethernet {

class NativeEthernetInterface : public Interface {
public:
    NativeEthernetInterface(ftl::ipv4::Address address, ftl::ipv4::Mask mask)
      : ethernet::Interface{address, mask} {}
    ~NativeEthernetInterface() override = default;

    ftl::ipv4::udp::SocketPtr CreateUdpSocket() override;
};

}  // namespace ftl::ethernet
