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

    constexpr uint16_t kReceiverPort    = 9382U;
    const Address      kLocalAddress{"127.0.0.1"};
    // We will use 239.0.0.42 as our multicast “test” group:
    const Address      kMulticastGroup{239, 0, 0, 42};

    // “Loopback” interface just for IPv4 on 127.0.0.1/24
    ftl::ethernet::NativeEthernetInterface lo{ Address{"127.0.0.1"}, Mask{"255.255.255.0"} };

    ftl::DataFrame::initialize(kAllocator);

    // Create a sender‐socket bound to the “lo” interface:
    SocketPtr sender = lo.CreateUdpSocket();

    if (!sender->open(4)) {
        std::cerr << "Failed to open sender socket\n";
        return 1;
    }

    // ← ADD THIS: Bind to loopback (port = 0).  This runs all of
    // the steps (bind to 127.0.0.1:0, setsockopt(IP_MULTICAST_TTL), setsockopt(IP_MULTICAST_IF), …)
    if (!sender->bind(0)) {
        std::cerr << "Failed to bind sender to loopback\n";
        return 1;
    }

    // Pre‐construct the multicast endpoint: 239.0.0.42:54321
    Endpoint multiDst{ kMulticastGroup, kReceiverPort };

    std::cout << "Starting send‐only loop (127.0.0.1 → 239.0.0.42:"
              << kReceiverPort << ")\n";

    while (true) {
        const char *msg_multicast = "Hello multicast";
        size_t      len_m         = std::strlen(msg_multicast);
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

        // Give the kernel a moment to emit and loop back the packet
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Wait a bit before sending again
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return 0;
}
