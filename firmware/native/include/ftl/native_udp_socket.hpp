// native_udp_socket.hpp
#pragma once

#include <cstddef>
#include <cstdint>

#include "ftl/ipv4/udp/socket.hpp"
#include "ftl/ipv4/udp/payload.hpp"
#include "ftl/ipv4/endpoint.hpp"
#include "ftl/native_ethernet_interface.hpp"

namespace ftl::ipv4::udp {

class NativeUdpSocket final : public Socket {
public:
  NativeUdpSocket(ftl::ethernet::NativeEthernetInterface &interface) 
    : interface_{interface} {}
  ~NativeUdpSocket() override;

  bool open(std::size_t receive_queue_len = 1) override;
  bool is_open() const noexcept override;
  bool bind(uint16_t port = 0) override;
  bool send(Payload payload, const ipv4::Endpoint dest) override;
  Payload receive(ipv4::Endpoint *const peer) override;
  void close() override;

  bool join_multicast_group(const ftl::ipv4::Address &group) override;
  bool leave_multicast_group(const ftl::ipv4::Address &group) override;

private:
  int fd_{-1};
  ftl::ethernet::NativeEthernetInterface &interface_;
};

}  // namespace ftl::ipv4::udp
