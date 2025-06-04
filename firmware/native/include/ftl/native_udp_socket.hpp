// native_udp_socket.hpp
#pragma once

#include <cstddef>
#include <cstdint>

#include "ftl/ipv4/udp/socket.hpp"
#include "ftl/ipv4/udp/payload.hpp"
#include "ftl/ipv4/endpoint.hpp"
#include "ftl/native_ethernet_interface.hpp"

namespace ftl::ipv4::udp {

//------------------------------------------------------------------------------
// NativeUdpSocket
//
// This class uses two separate socket descriptors (tx_fd_ and rx_multicast_fd_) 
// to ensure that both sending and receiving of multicast traffic can be forced 
// onto a specific local interface (for example, the loopback interface) so that 
// you can observe the packets in Wireshark. 
//
// Overview:
//
// 1. tx_fd_ (Transmit Socket)
//    • Bound explicitly to the chosen interface’s IP address (e.g., 127.0.0.1) 
//      and the UDP port. 
//    • IP_MULTICAST_IF is set to that same interface, causing all multicast 
//      egress to travel out on that interface.
//    • IP_MULTICAST_LOOP is enabled so that packets sent on loopback will 
//      loop back to any socket that has joined the group on that interface. 
//    • Because tx_fd_ is bound to the interface’s address:port, any attempt to 
//      receive on loopback (e.g., “127.0.0.1:port”) will also catch looped-back 
//      packets if a socket is bound there.
//
// 2. rx_multicast_fd_ (Receive Socket)
//    • Bound to INADDR_ANY on the same UDP port, with SO_REUSEADDR (and 
//      optionally SO_REUSEPORT) enabled so that it can share the port with 
//      tx_fd_. 
//    • Joins the multicast group on exactly the same interface address used 
//      by tx_fd_. That ensures the kernel delivers looped-back group packets 
//      to rx_multicast_fd_. 
//    • Because it is bound to INADDR_ANY:port, it will receive both multicast 
//      packets (for which it has joined the group) and any unicast packets 
//      addressed specifically to the interface’s IP:port.
//
// Why two descriptors?
// --------------------
// • If you bind a single socket to INADDR_ANY:port and then set IP_MULTICAST_IF 
//   to loopback, the kernel may choose a different interface for RX, or you 
//   may not see your own looped-back packets in Wireshark on lo. 
// • By having tx_fd_ bound to the interface IP:port, all outgoing multicast is 
//   forced onto that interface. Wireshark sees it on lo (or whichever IF you pick). 
// • By having rx_multicast_fd_ bound to INADDR_ANY:port with a specific 
//   IP_ADD_MEMBERSHIP on the same interface, you are guaranteed to receive 
//   loop-backed multicast on that interface. 
// • Then, at runtime, we do a non-blocking select() on both tx_fd_ and 
//   rx_multicast_fd_. Any unicast (to interface IP:port) shows up on tx_fd_, 
//   any multicast shows up on rx_multicast_fd_. We return whichever arrives first.
//
// Potential side effects and caveats:
// -----------------------------------
// • Both descriptors share the same UDP port. You **must** enable SO_REUSEADDR 
//   (and on some platforms SO_REUSEPORT) before calling bind() on each socket, 
//   or the second bind() will fail with “Address already in use.” 
// • Because rx_multicast_fd_ is bound to INADDR_ANY, it will also see any 
//   unicast datagrams sent to the interface’s IP:port. If you only expect 
//   multicast, be prepared to filter out any stray unicast traffic. 
// • Packet ordering is not guaranteed between the two sockets. If both a 
//   multicast and a unicast arrive at the same time, select() will deliver them 
//   in whichever order the kernel detects. Your application logic should handle 
//   potential interleaving. 
// • Slightly higher resource usage (two file descriptors per socket object), 
//   and extra complexity in receive() because of the select() over two fds. 
// • If you change the interface after bind(), you must re-join the multicast group 
//   and/or re-bind tx_fd_ so that IP_MULTICAST_IF matches. Mismatches can cause 
//   multicast packets not to loop back or not be received. 
// • Be cautious about IPv4 vs IPv6: this class is IPv4-only. If you need IPv6 
//   multicast, a different approach is required.
//
// In summary, the dual-descriptor approach is necessary to force multicast egress 
// onto a chosen interface (so you can inspect it in Wireshark) while still 
// receiving multicast (and unicast) on the same UDP port. Keep in mind the 
// requirements for port reuse, correct interface choice in IP_ADD_MEMBERSHIP, 
// and the implications of binding to INADDR_ANY vs a specific IP.
//
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
  ftl::ethernet::NativeEthernetInterface &interface_;
  int tx_fd_{ -1 };
  int rx_multicast_fd_{ -1 };
  static constexpr int OVERRIDE_TTL = 16;
};

}  // namespace ftl::ipv4::udp
