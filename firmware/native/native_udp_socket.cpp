// native_udp_socket.cpp
#include "ftl/native_udp_socket.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>

#include <iostream>

namespace ftl::ipv4::udp {

NativeUdpSocket::NativeUdpSocket() = default;

NativeUdpSocket::~NativeUdpSocket() {
  close();
}

bool NativeUdpSocket::open(std::size_t receive_queue_len) {
  if (fd_ >= 0) {
    return false;  // already open
  }
  fd_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (fd_ < 0) {
    return false;
  }

  // allow immediate rebinding of the same address/port
  int opt = 1;
  ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // UDP receive queue length is in *datagrams*, but SO_RCVBUF is in *bytes*.
  // To hold receive_queue_len datagrams of up to kMaxDatagram bytes each:
  constexpr int kMaxDatagram = 65507;  // Maximum safe UDP payload
  int buf_bytes = static_cast<int>(receive_queue_len) * kMaxDatagram;
  ::setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &buf_bytes, sizeof(buf_bytes));

  // Make the socket non-blocking
  int flags = fcntl(fd_, F_GETFL, 0);
  if (flags < 0) return false;
  if (fcntl(fd_, F_SETFL, flags | O_NONBLOCK) < 0) return false;

  return true;
}

bool NativeUdpSocket::is_open() const noexcept {
  return fd_ >= 0;
}

bool NativeUdpSocket::bind(uint16_t port) {
  if (!is_open()) {
    return false;
  }
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);
  if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    return false;
  }
  return true;
}

bool NativeUdpSocket::send(Payload payload, const ipv4::Endpoint dest) {
  if (!is_open()) {
    return false;
  }
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = dest.address().ToUint32();
  addr.sin_port = htons(dest.port());
  auto sent = ::sendto(fd_,
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

  // This will block until a full datagram arrives, then return its exact length.
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
  peer->set_address(Address{ addr.sin_addr.s_addr });
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
  mreq.imr_multiaddr.s_addr = group.ToUint32();
  mreq.imr_interface.s_addr = INADDR_ANY;
  return (::setsockopt(fd_, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                      &mreq, sizeof(mreq)) == 0);
}

bool NativeUdpSocket::leave_multicast_group(const ftl::ipv4::Address &group) {
  if (!is_open()) {
    return false;
  }
  ip_mreq mreq{};
  mreq.imr_multiaddr.s_addr = group.ToUint32();
  mreq.imr_interface.s_addr = INADDR_ANY;
  return (::setsockopt(fd_, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                      &mreq, sizeof(mreq)) == 0);
}

}  // namespace ftl::ipv4::udp
