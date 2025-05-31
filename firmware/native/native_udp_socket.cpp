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

// Destructor: ensure both TX and RX sockets are closed when the object is destroyed.
NativeUdpSocket::~NativeUdpSocket() {
    close();
}

// open(): create two sockets—one for transmit (tx_fd_) and one for receive (rx_multicast_fd_).
// Returns false if either socket already exists or if socket() fails.
bool NativeUdpSocket::open(std::size_t /* receive_queue_len */) {
    // If either descriptor is already open, we fail.
    if (tx_fd_ >= 0 || rx_multicast_fd_ >= 0) {
        return false;
    }

    // 1) Create the transmit socket as IPv4 UDP.
    tx_fd_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (tx_fd_ < 0) {
        return false;
    }

    // 2) Create the receive socket as IPv4 UDP.
    rx_multicast_fd_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (rx_multicast_fd_ < 0) {
        // If RX creation fails, close TX and return false.
        ::close(tx_fd_);
        tx_fd_ = -1;
        return false;
    }

    return true;
}

// is_open(): return true only if both TX and RX descriptors are valid (>= 0).
bool NativeUdpSocket::is_open() const noexcept {
    return (tx_fd_ >= 0 && rx_multicast_fd_ >= 0);
}

// bind(): bind both RX and TX sockets to the given port. RX is bound to INADDR_ANY,
// TX is bound to the interface’s IP. Enable port reuse on both sockets to allow
// two sockets sharing the same port. Also set nonblocking and multicast options.
bool NativeUdpSocket::bind(uint16_t port) {
    if (!is_open()) {
        return false;
    }

    // Enable SO_REUSEADDR and SO_REUSEPORT on the RX socket so it can bind to the same
    // port as TX.
    int opt = 1;
    if (::setsockopt(rx_multicast_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        return false;
    }
    if (::setsockopt(rx_multicast_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0)
    {
        return false;
    }

    // Bind RX socket to INADDR_ANY:port so that it can receive both unicast and
    // multicast traffic on any local address.
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

    // Set RX socket to nonblocking mode so receive() can poll without blocking.
    if (fcntl(rx_multicast_fd_, F_SETFL, O_NONBLOCK) < 0) {
        return false;
    }

    // Enable SO_REUSEADDR and SO_REUSEPORT on the TX socket so it can bind to the same port as RX.
    if (::setsockopt(tx_fd_,
                     SOL_SOCKET,
                     SO_REUSEADDR,
                     &opt,
                     sizeof(opt)) < 0)
    {
        return false;
    }

    if (::setsockopt(tx_fd_,
                     SOL_SOCKET,
                     SO_REUSEPORT,
                     &opt,
                     sizeof(opt)) < 0)
    {
        return false;
    }

    // Bind TX socket to the specific interface’s IP:port. This forces outgoing multicast to egress
    // on that interface so you can see it in Wireshark.
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

    // Set TX socket to nonblocking to match RX behavior.
    if (fcntl(tx_fd_, F_SETFL, O_NONBLOCK) < 0) {
        return false;
    }

    // Configure multicast TTL on TX (limiting how many routers can forward).
    int ttl = OVERRIDE_TTL;
    if (::setsockopt(tx_fd_,
                     IPPROTO_IP,
                     IP_MULTICAST_TTL,
                     &ttl,
                     sizeof(ttl)) < 0)
    {
        return false;
    }

    // Enable loopback for multicast so packets sent on this interface loop back to any socket
    // joined on the same group.
    unsigned char loop = 1;
    if (::setsockopt(tx_fd_,
                     IPPROTO_IP,
                     IP_MULTICAST_LOOP,
                     &loop,
                     sizeof(loop)) < 0)
    {
        return false;
    }

    // Tell the kernel to use this specific interface for multicast egress.
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

// send(): send a UDP packet (unicast or multicast) on the TX socket. Returns true
// if the full payload was sent successfully.
bool NativeUdpSocket::send(Payload payload, const ipv4::Endpoint dest) {
    if (tx_fd_ < 0) {
        return false;
    }

    // Build the destination sockaddr_in using dest.address() and dest.port().
    sockaddr_in dst_addr{};
    std::memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.sin_family      = AF_INET;
    dst_addr.sin_addr.s_addr = htonl(dest.address().ToUint32());
    dst_addr.sin_port        = htons(dest.port());

    // sendto() on tx_fd_. If the return doesn’t match payload.size(), treat as failure.
    ssize_t sent = ::sendto(tx_fd_,
                             payload.data(),
                             payload.size(),
                             0,
                             reinterpret_cast<const sockaddr*>(&dst_addr),
                             sizeof(dst_addr));
    return (sent == static_cast<ssize_t>(payload.size()));
}

// receive(): nonblocking attempt to read from either rx_multicast_fd_ or tx_fd_.
// Uses select() with zero timeout to poll both. Returns the first available packet,
// preferring multicast (rx_multicast_fd_) over unicast (tx_fd_). Empty Payload if none.
Payload NativeUdpSocket::receive(ipv4::Endpoint *const peer) {
    if (!is_open()) {
        return {};
    }

    // Prepare the fd_set for select: watch rx_multicast_fd_ and tx_fd_.
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(rx_multicast_fd_, &read_fds);
    FD_SET(tx_fd_, &read_fds);

    // The maximum file descriptor plus one for select().
    int max_fd = std::max(rx_multicast_fd_, tx_fd_);
    timeval timeout{0, 0};  // zero timeout => nonblocking

    int ready = ::select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);
    if (ready < 0) {
        // Real error occurred in select(); return empty.
        return {};
    }
    if (ready == 0) {
        // No socket is ready; return empty immediately.
        return {};
    }

    // Determine which descriptor has data. Prefer RX (multicast/unicast via INADDR_ANY)
    int which_fd = -1;
    if (FD_ISSET(rx_multicast_fd_, &read_fds)) {
        which_fd = rx_multicast_fd_;
    } else if (FD_ISSET(tx_fd_, &read_fds)) {
        which_fd = tx_fd_;
    }

    if (which_fd < 0) {
        // Unexpected: no FD was set even though ready > 0. Return empty.
        return {};
    }

    // Prepare a buffer for the worst-case UDP payload (65507 bytes).
    constexpr std::size_t kMaxDatagram = 65507;
    uint8_t tmpBuf[kMaxDatagram];
    sockaddr_in addr{};
    socklen_t addrlen = sizeof(addr);

    // Perform recvfrom() on the chosen FD.
    ssize_t n = ::recvfrom(which_fd,
                           tmpBuf,
                           sizeof(tmpBuf),
                           0,
                           reinterpret_cast<sockaddr*>(&addr),
                           &addrlen);
    if (n < 0) {
        // If it would block, simply return empty. Otherwise, real error.
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return {};
        }
        return {};
    }

    // Copy the received data into a Payload of exactly the right size.
    size_t len = static_cast<size_t>(n);
    Payload p(len);
    std::memcpy(p.data(), tmpBuf, len);

    // Populate the peer endpoint (host-order address and port).
    peer->set_address(Address{ ntohl(addr.sin_addr.s_addr) });
    peer->set_port(static_cast<uint16_t>(ntohs(addr.sin_port)));

    return p;
}

// close(): close both TX and RX sockets if they are open, and set FDs to -1.
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

// join_multicast_group(): instruct the RX socket to join a specific multicast group
// on the interface that was bound to TX. Returns true if setsockopt succeeds.
bool NativeUdpSocket::join_multicast_group(const ftl::ipv4::Address &group) {
    if (rx_multicast_fd_ < 0) {
        return false;
    }

    ip_mreq mreq{};
    // Multicast address in network byte order
    mreq.imr_multiaddr.s_addr = htonl(group.ToUint32());

    // Interface must match the one used for TX, so loopback or whatever was bound.
    uint32_t iface_be = htonl(interface_.address().ToUint32());
    mreq.imr_interface.s_addr = iface_be;

    // IP_ADD_MEMBERSHIP tells the kernel to deliver that group’s packets for RX.
    return (::setsockopt(rx_multicast_fd_,
                         IPPROTO_IP,
                         IP_ADD_MEMBERSHIP,
                         &mreq,
                         sizeof(mreq)) == 0);
}

// leave_multicast_group(): drop the RX socket’s membership in a multicast group
// on the same interface. Must use the identical interface address used in join.
bool NativeUdpSocket::leave_multicast_group(const ftl::ipv4::Address &group) {
    if (rx_multicast_fd_ < 0) {
        return false;
    }

    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = htonl(group.ToUint32());

    // Must drop on the same interface used in join.
    uint32_t iface_be = htonl(interface_.address().ToUint32());
    mreq.imr_interface.s_addr = iface_be;

    return (::setsockopt(rx_multicast_fd_,
                         IPPROTO_IP,
                         IP_DROP_MEMBERSHIP,
                         &mreq,
                         sizeof(mreq)) == 0);
}

}  // namespace ftl::ipv4::udp
