#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

#include "ftl/native_ethernet_interface.hpp"
#include "ftl/bump_allocator.hpp"
#include "ftl/native_udp_socket.hpp"
#include "ftl/ipv4/endpoint.hpp"
#include "ftl/ipv4/udp/payload.hpp"

static constexpr size_t POOL_MEMORY_SIZE = 16 * 1024;
uint8_t kBumpBuffer[POOL_MEMORY_SIZE];
ftl::BumpAllocator kAllocator{kBumpBuffer, POOL_MEMORY_SIZE};

int main() {
    using namespace ftl::ipv4;
    using namespace ftl::ipv4::udp;

    constexpr uint16_t kReceiverPort = 54321;
    const Address      kLocalAddress{"127.0.0.1"};
    // We will use 239.0.0.42 as our multicast “test” group:
    const Address      kMulticastGroup{239, 0, 0, 42};

    // “Loopback” interface just for IPv4 on 127.0.0.1/24
    ftl::ethernet::NativeEthernetInterface lo{ Address{"127.0.0.1"}, Mask{"255.255.255.0"} };

    ftl::DataFrame::initialize(kAllocator);

    // Create a sender‐socket and a receiver‐socket, bound to the “lo” interface:
    SocketPtr sender = lo.CreateUdpSocket();
    SocketPtr receiver = lo.CreateUdpSocket();

    if (!sender->open(4)) {
        std::cerr << "Failed to open sender socket\n";
        return 1;
    }
    if (!receiver->open(4)) {
        std::cerr << "Failed to open receiver socket\n";
        return 1;
    }

    // Bind the receiver to 0.0.0.0:kReceiverPort on the “lo” interface,
    // then join the multicast group 239.0.0.42:
    if (!receiver->bind(kReceiverPort)) {
        std::cerr << "Failed to bind receiver to port " << kReceiverPort << "\n";
        return 1;
    }
    if (!receiver->join_multicast_group(kMulticastGroup)) {
        std::cerr << "Failed to join multicast group " << kMulticastGroup.ToString().c_str() << "\n";
        return 1;
    }

    // Pre‐construct two endpoints: unicast→127.0.0.1:kReceiverPort, and multicast→239.0.0.42:kReceiverPort
    Endpoint unicastDst{ kLocalAddress,   kReceiverPort };
    Endpoint multiDst{  kMulticastGroup, kReceiverPort };

    std::cout << "Starting send/receive loop on 127.0.0.1:" << kReceiverPort
              << " (joined multicast " << kMulticastGroup.ToString().c_str() << ")\n";

    while (true) {
        // --- First: send a unicast packet “Hello unicast” ---
        {
            const char *msg_unicast = "Hello unicast";
            size_t      len_u = std::strlen(msg_unicast);
            Payload     p_send_u(len_u);
            std::memcpy(p_send_u.data(), msg_unicast, len_u);

            bool ok = sender->send(std::move(p_send_u), unicastDst);
            if (!ok) {
                std::cerr << "[send] unicast error, errno=" << errno
                          << " (" << std::strerror(errno) << ")\n";
            } else {
                std::cout << "[send] unicast: “" << msg_unicast 
                          << "” → " << unicastDst.address().ToString().c_str()
                          << ":" << unicastDst.port() << "\n";
            }
        }

        // --- Then: send a multicast packet “Hello multicast” ---
        {
            const char *msg_multicast = "Hello multicast";
            size_t      len_m = std::strlen(msg_multicast);
            Payload     p_send_m(len_m);
            std::memcpy(p_send_m.data(), msg_multicast, len_m);

            bool ok = sender->send(std::move(p_send_m), multiDst);
            if (!ok) {
                std::cerr << "[send] multicast error, errno=" << errno
                          << " (" << std::strerror(errno) << ")\n";
            } else {
                std::cout << "[send] multicast: “" << msg_multicast 
                          << "” → " << multiDst.address().ToString().c_str()
                          << ":" << multiDst.port() << "\n";
            }
        }

        // Give the kernel a moment to loop back the packets:
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Attempt to read *all* pending packets (unicast + multicast) until none remain.
        // Because we opened the socket non‐blocking, receive() will return empty
        // once the queue is drained.
        while (true) {
            Endpoint peer{};
            Payload  p_recv = receiver->receive(&peer);
            if (!p_recv) {
                break; // no more packets in the queue right now
            }
            std::string received{ reinterpret_cast<char*>(p_recv.data()), p_recv.size() };
            std::cout << "[recv] " 
                      << peer.address().ToString().c_str() 
                      << ":" << peer.port()
                      << " → “" << received << "”\n";
        }

        // Wait a little before sending the next pair:
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return 0;
}
