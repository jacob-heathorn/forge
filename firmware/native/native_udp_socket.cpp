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

/// Bind to the loopback interface on port 0 (ephemeral).
/// Then set TTL and multicast‐IF exactly as in the working example.
bool NativeUdpSocket::bind(uint16_t /*port*/) {
    if (!is_open()) {
        return false;
    }

    // 1) Bind to <loopback>:0
    in_addr tmp;
    if (::inet_pton(AF_INET, "127.0.0.1", &tmp) != 1) {
        // Should never fail for "127.0.0.1"
        return false;
    }
    uint32_t loopback_be = tmp.s_addr; // already network‐order

    sockaddr_in bind_addr;
    std::memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family      = AF_INET;
    bind_addr.sin_addr.s_addr = loopback_be;
    bind_addr.sin_port        = htons(0);

    if (::bind(fd_, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
        return false;
    }

    // 2) Set multicast TTL = OVERRIDE_TTL
    int ttl = OVERRIDE_TTL;
    if (::setsockopt(fd_,
                     IPPROTO_IP,
                     IP_MULTICAST_TTL,
                     &ttl,
                     sizeof(ttl)) < 0)
    {
        return false;
    }

    // 3) Tell kernel to use loopback for multicast egress
    //    We already have loopback_be in network‐order.
    if (::setsockopt(fd_,
                     IPPROTO_IP,
                     IP_MULTICAST_IF,
                     &loopback_be,
                     sizeof(loopback_be)) < 0)
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

void NativeUdpSocket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

// (Receive and multicast‐join/drop methods can remain unchanged or be removed if not needed.)
// For this minimal send‐only example, we leave them stubbed out:

Payload NativeUdpSocket::receive(ipv4::Endpoint* const /*peer*/) {
    // Not used in “send‐only” scenario.
    return {};
}

bool NativeUdpSocket::join_multicast_group(const ftl::ipv4::Address& /*group*/) {
    // Not used in “send‐only” scenario.
    return false;
}

bool NativeUdpSocket::leave_multicast_group(const ftl::ipv4::Address& /*group*/) {
    // Not used in “send‐only” scenario.
    return false;
}

} // namespace ftl::ipv4::udp
