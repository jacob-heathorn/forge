// native_udp_socket.cpp
//
// Modified to match the minimal working “Hello multicast” example:
//   • Bind to the loopback interface (e.g. 127.0.0.1:0).
//   • Set IP_MULTICAST_TTL.
//   • Set IP_MULTICAST_IF to loopback.
//   • Provide a simple send-only path (no nonblocking, no receive setup).
//
// Build with C++17.

#include "ftl/native_udp_socket.hpp"

#include <arpa/inet.h>   // inet_pton, htonl, htons
#include <fcntl.h>       // fcntl, O_NONBLOCK
#include <netinet/in.h>
#include <sys/socket.h>  // socket, setsockopt, bind, sendto
#include <unistd.h>      // close
#include <cstring>
#include <iostream>

#define OVERRIDE_TTL 16

namespace ftl::ipv4::udp {

NativeUdpSocket::~NativeUdpSocket() {
    close();
}

bool NativeUdpSocket::open(std::size_t /*unused*/) {
    if (fd_ >= 0) {
        return false; // already open
    }
    fd_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd_ < 0) {
        return false;
    }
    return true;
}

bool NativeUdpSocket::is_open() const noexcept {
    return fd_ >= 0;
}

bool NativeUdpSocket::bind(uint16_t port) {
  if (!is_open()) {
      return false;
  }

  // Get the interface address in network order:
  uint32_t interface_addr = htonl(this->interface_.address().ToUint32());

  // Bind the socket to interface:port
  sockaddr_in bind_addr{};
  bind_addr.sin_family      = AF_INET;
  bind_addr.sin_addr.s_addr = interface_addr;
  bind_addr.sin_port        = htons(port);
  if (::bind(fd_, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
      return false;
  }

  if (fcntl(fd_, F_SETFL, O_NONBLOCK) < 0) {
    return false;
  }

  // Set the multicast TTL to OVERRIDE_TTL
  int ttl = OVERRIDE_TTL;
  if (::setsockopt(fd_,
                   IPPROTO_IP,
                   IP_MULTICAST_TTL,
                   &ttl,
                   sizeof(ttl)) < 0)
  {
      return false;
  }

  // Enable loopback of multicast packets (so that a packet you send to 239.0.0.42 on lo will also
  //    be delivered back to any socket on lo that's in that group).
  unsigned char loop = 1;
  if (::setsockopt(fd_,
                    IPPROTO_IP,
                    IP_MULTICAST_LOOP,
                    &loop,
                    sizeof(loop)) < 0)
  {
      return false;
  }

  // Tell the kernel to use this loopback interface for multicast egress
  if (::setsockopt(fd_,
                   IPPROTO_IP,
                   IP_MULTICAST_IF,
                   &interface_addr,
                   sizeof(interface_addr)) < 0)
  {
      return false;
  }

  return true;
}

bool NativeUdpSocket::send(Payload payload, const ipv4::Endpoint dest) {
    if (!is_open()) {
        return false;
    }

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(dest.address().ToUint32());
    addr.sin_port        = htons(dest.port());

    ssize_t sent = ::sendto(fd_,
                            payload.data(),
                            payload.size(),
                            0,
                            reinterpret_cast<sockaddr*>(&addr),
                            sizeof(addr));
    return sent == static_cast<ssize_t>(payload.size());
}

Payload NativeUdpSocket::receive(ipv4::Endpoint *const peer) {
  if (!is_open()) {
    return Payload(0);
  }

  constexpr std::size_t kMaxDatagram = 65507;  // worst‐case UDP payload
  uint8_t               tmpBuf[kMaxDatagram];
  sockaddr_in           addr{};
  socklen_t             addrlen = sizeof(addr);

  ssize_t n = ::recvfrom(fd_, tmpBuf, sizeof(tmpBuf), 0,
                       (sockaddr*)&addr, &addrlen);
  if (n < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // no data ready, return empty immediately
      return {};
    }
    // real error
    return {};
  }

  // Allocate a Payload sized exactly to the received length
  size_t len = static_cast<size_t>(n);
  Payload p(len);
  std::memcpy(p.data(), tmpBuf, len);

  // Fill in peer info (Address ctor takes network-order uint32_t)
  peer->set_address(Address{ ntohl(addr.sin_addr.s_addr) });
  peer->set_port   ( ntohs(addr.sin_port) );

  return p;
}

void NativeUdpSocket::close() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

bool NativeUdpSocket::join_multicast_group(const ftl::ipv4::Address &group) {
  if (!is_open()) {
      return false;
  }

  ip_mreq mreq{};
  mreq.imr_multiaddr.s_addr = htonl(group.ToUint32());

  // ← Here we explicitly use loopback (“127.0.0.1”) instead of INADDR_ANY:
  uint32_t loopback_be = htonl(interface_.address().ToUint32());
  mreq.imr_interface.s_addr = loopback_be;

  return (::setsockopt(fd_,
                      IPPROTO_IP,
                      IP_ADD_MEMBERSHIP,
                      &mreq,
                      sizeof(mreq)) == 0);
}

bool NativeUdpSocket::leave_multicast_group(const ftl::ipv4::Address &group) {
  if (!is_open()) {
    return false;
  }
  ip_mreq mreq{};
  mreq.imr_multiaddr.s_addr = htonl(group.ToUint32());
  mreq.imr_interface.s_addr = INADDR_ANY;
  return (::setsockopt(fd_, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                      &mreq, sizeof(mreq)) == 0);
}

} // namespace ftl::ipv4::udp
