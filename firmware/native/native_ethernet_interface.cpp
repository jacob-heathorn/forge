// src/native_ethernet_interface.cpp
#include "ftl/native_ethernet_interface.hpp"
#include "ftl/native_udp_socket.hpp"
#include "ftl/deleter.hpp"

namespace ftl::ethernet {

ftl::ipv4::udp::SocketPtr NativeEthernetInterface::CreateUdpSocket() {
  // one shared DefaultDeleter instance lives for the program duration
  static DefaultDeleter<ftl::ipv4::udp::Socket> defaultDeleter{};
  return ftl::ipv4::udp::SocketPtr{
    new ftl::ipv4::udp::NativeUdpSocket(*this),
    DelegatingDeleter<ftl::ipv4::udp::Socket>{ &defaultDeleter }
  };
}

}  // namespace ftl::ethernet
