// src/native_ethernet_interface.cpp
#include "forge/native/ethernet_interface.hpp"
#include "forge/native/udp_socket.hpp"
#include "forge/ftl/memory.hpp"

namespace ftl::ethernet {

ftl::ipv4::udp::SocketPtr NativeEthernetInterface::CreateUdpSocket() {
  // one shared DefaultDeleter instance lives for the program duration
  static ftl::DefaultDeleter<ftl::ipv4::udp::Socket> defaultDeleter{};
  return ftl::ipv4::udp::SocketPtr{
    new ftl::ipv4::udp::NativeUdpSocket(*this),
    &defaultDeleter
  };
}

}  // namespace ftl::ethernet
