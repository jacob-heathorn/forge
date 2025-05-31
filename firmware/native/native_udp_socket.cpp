// native_udp_socket.cpp

#include "ftl/native_udp_socket.hpp"

#include <arpa/inet.h>    // inet_pton, htonl, htons
#include <fcntl.h>        // fcntl, O_NONBLOCK
#include <netinet/in.h>   // sockaddr_in, IPPROTO_IP, IP_MULTICAST_TTL, IP_MULTICAST_LOOP, IP_MULTICAST_IF, ip_mreq
#include <sys/select.h>   // select, fd_set
#include <sys/socket.h>   // socket, setsockopt, bind, sendto, recvfrom
#include <unistd.h>       // close
#include <cerrno>
#include <cstring>

namespace ftl::ipv4::udp {

NativeUdpSocket::~NativeUdpSocket() {
    close();
}

bool NativeUdpSocket::open(std::size_t /* receive_queue_len */) {
    // If either FD is already open, fail
    if (tx_fd_ >= 0 || rx_multicast_fd_ >= 0) {
        return false;
    }

    // 1) Create TX socket
    tx_fd_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (tx_fd_ < 0) {
        return false;
    }

    // 2) Create RX socket
    rx_multicast_fd_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (rx_multicast_fd_ < 0) {
        ::close(tx_fd_);
        tx_fd_ = -1;
        return false;
    }

    return true;
}

bool NativeUdpSocket::is_open() const noexcept {
    return (tx_fd_ >= 0 && rx_multicast_fd_ >= 0);
}

bool NativeUdpSocket::bind(uint16_t port) {
  if (!is_open()) {
      return false;
  }

  // 1) Enable port reuse on RX socket
  int opt = 1;
  if (::setsockopt(rx_multicast_fd_,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &opt,
                   sizeof(opt)) < 0)
  {
      return false;
  }
  // (Optional, if your distro/kernel requires it for true port sharing)
  #ifdef SO_REUSEPORT
  if (::setsockopt(rx_multicast_fd_,
                   SOL_SOCKET,
                   SO_REUSEPORT,
                   &opt,
                   sizeof(opt)) < 0)
  {
      return false;
  }
  #endif

  // Now bind RX to INADDR_ANY:port
  sockaddr_in rx_addr{};
  rx_addr.sin_family      = AF_INET;
  rx_addr.sin_port        = htons(port);
  rx_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (::bind(rx_multicast_fd_,
             reinterpret_cast<sockaddr*>(&rx_addr),
             sizeof(rx_addr)) < 0)
  {
      return false;
  }

  if (fcntl(rx_multicast_fd_, F_SETFL, O_NONBLOCK) < 0) {
      return false;
  }

  // 2) Enable port reuse on TX socket
  if (::setsockopt(tx_fd_,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &opt,
                   sizeof(opt)) < 0)
  {
      return false;
  }
  #ifdef SO_REUSEPORT
  if (::setsockopt(tx_fd_,
                   SOL_SOCKET,
                   SO_REUSEPORT,
                   &opt,
                   sizeof(opt)) < 0)
  {
      return false;
  }
  #endif

  // Bind TX to (interface IP):port
  uint32_t iface_be = htonl(interface_.address().ToUint32());
  sockaddr_in tx_addr{};
  tx_addr.sin_family      = AF_INET;
  tx_addr.sin_port        = htons(port);
  tx_addr.sin_addr.s_addr = iface_be;

  if (::bind(tx_fd_,
             reinterpret_cast<sockaddr*>(&tx_addr),
             sizeof(tx_addr)) < 0)
  {
      return false;
  }

  if (fcntl(tx_fd_, F_SETFL, O_NONBLOCK) < 0) {
      return false;
  }

  // 3) Set multicast TTL, loopback, and IF on TX
  int ttl = OVERRIDE_TTL;
  if (::setsockopt(tx_fd_,
                   IPPROTO_IP,
                   IP_MULTICAST_TTL,
                   &ttl,
                   sizeof(ttl)) < 0)
  {
      return false;
  }

  unsigned char loop = 1;
  if (::setsockopt(tx_fd_,
                   IPPROTO_IP,
                   IP_MULTICAST_LOOP,
                   &loop,
                   sizeof(loop)) < 0)
  {
      return false;
  }

  if (::setsockopt(tx_fd_,
                   IPPROTO_IP,
                   IP_MULTICAST_IF,
                   &iface_be,
                   sizeof(iface_be)) < 0)
  {
      return false;
  }

  return true;
}

bool NativeUdpSocket::send(Payload payload, const ipv4::Endpoint dest) {
    if (tx_fd_ < 0) {
        return false;
    }

    sockaddr_in dst_addr{};
    std::memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.sin_family      = AF_INET;
    dst_addr.sin_addr.s_addr = htonl(dest.address().ToUint32());
    dst_addr.sin_port        = htons(dest.port());

    ssize_t sent = ::sendto(tx_fd_,
                             payload.data(),
                             payload.size(),
                             0,
                             reinterpret_cast<const sockaddr*>(&dst_addr),
                             sizeof(dst_addr));
    return (sent == static_cast<ssize_t>(payload.size()));
}

Payload NativeUdpSocket::receive(ipv4::Endpoint *const peer) {
    if (!is_open()) {
        return Payload(0);
    }

    // Build fd_set for select
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(rx_multicast_fd_, &read_fds);
    FD_SET(tx_fd_, &read_fds);

    int max_fd = std::max(rx_multicast_fd_, tx_fd_);
    timeval timeout{0, 0};  // zero timeout => non‐blocking

    int ready = ::select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
    if (ready < 0) {
        // Real error
        return Payload(0);
    }
    if (ready == 0) {
        // No data on either socket
        return Payload(0);
    }

    // Prefer RX if both have data
    int which_fd = -1;
    if (FD_ISSET(rx_multicast_fd_, &read_fds)) {
        which_fd = rx_multicast_fd_;
    } else if (FD_ISSET(tx_fd_, &read_fds)) {
        which_fd = tx_fd_;
    }

    if (which_fd < 0) {
        return Payload(0);
    }

    constexpr std::size_t kMaxDatagram = 65507;
    uint8_t tmpBuf[kMaxDatagram];
    sockaddr_in addr{};
    socklen_t addrlen = sizeof(addr);

    ssize_t n = ::recvfrom(which_fd,
                           tmpBuf,
                           sizeof(tmpBuf),
                           0,
                           reinterpret_cast<sockaddr*>(&addr),
                           &addrlen);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Would block ⇒ no data
            return Payload(0);
        }
        // Real error
        return Payload(0);
    }

    size_t len = static_cast<size_t>(n);
    Payload p(len);
    std::memcpy(p.data(), tmpBuf, len);

    peer->set_address(Address{ ntohl(addr.sin_addr.s_addr) });
    peer->set_port(static_cast<uint16_t>(ntohs(addr.sin_port)));

    return p;
}

void NativeUdpSocket::close() {
    if (tx_fd_ >= 0) {
        ::close(tx_fd_);
        tx_fd_ = -1;
    }
    if (rx_multicast_fd_ >= 0) {
        ::close(rx_multicast_fd_);
        rx_multicast_fd_ = -1;
    }
}

bool NativeUdpSocket::join_multicast_group(const ftl::ipv4::Address &group) {
  if (rx_multicast_fd_ < 0) { return false; }

  ip_mreq mreq{};
  mreq.imr_multiaddr.s_addr = htonl(group.ToUint32());

  uint32_t iface_be = htonl(interface_.address().ToUint32());
  mreq.imr_interface.s_addr = iface_be;

  return (::setsockopt(rx_multicast_fd_,
                       IPPROTO_IP,
                       IP_ADD_MEMBERSHIP,
                       &mreq,
                       sizeof(mreq)) == 0);
}

bool NativeUdpSocket::leave_multicast_group(const ftl::ipv4::Address &group) {
  if (rx_multicast_fd_ < 0) {
      return false;
  }

  ip_mreq mreq{};
  mreq.imr_multiaddr.s_addr = htonl(group.ToUint32());

  // Must drop on the *same* interface we joined on:
  uint32_t iface_be = htonl(interface_.address().ToUint32());
  mreq.imr_interface.s_addr = iface_be;

  return (::setsockopt(rx_multicast_fd_,
                       IPPROTO_IP,
                       IP_DROP_MEMBERSHIP,
                       &mreq,
                       sizeof(mreq)) == 0);
}

}  // namespace ftl::ipv4::udp
