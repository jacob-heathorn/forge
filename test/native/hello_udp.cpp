#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

#include "ftl/native_ethernet_interface.hpp"
#include "ftl/allocator/bump_allocator.hpp"
#include "ftl/native_udp_socket.hpp"
#include "ftl/ipv4/endpoint.hpp"
#include "ftl/ipv4/udp/payload.hpp"
#include "ftl/allocator/bump_pool_buffer_strategy.hpp"
#include "ftl/allocator/buffer_allocator.hpp"

static constexpr size_t POOL_MEMORY_SIZE = 16 * 1024;
uint8_t kBumpBuffer[POOL_MEMORY_SIZE];
ftl::BumpAllocator kAllocator{kBumpBuffer, POOL_MEMORY_SIZE};

int main() {
    using namespace ftl::ipv4;
    using namespace ftl::ipv4::udp;

    constexpr uint16_t kSenderPort   = 5555;
    constexpr uint16_t kReceiverPort = 9382U;
    const Address      kLocalAddress{"127.0.0.1"};
    const Address      kMulticastGroup{239, 0, 0, 42};

    // "Loopback" interface just for IPv4 on 127.0.0.1/24
    ftl::ethernet::NativeEthernetInterface lo{ Address{"127.0.0.1"}, Mask{"255.255.255.0"} };

    // Initialize Payload allocator with bump pool strategies
    // Create individual strategies for each buffer size
    static ftl::allocator::BumpPoolBufferStrategy strategy256(kAllocator, 256);
    static ftl::allocator::BumpPoolBufferStrategy strategy512(kAllocator, 512);
    static ftl::allocator::BumpPoolBufferStrategy strategy768(kAllocator, 768);
    static ftl::allocator::BumpPoolBufferStrategy strategy1024(kAllocator, 1024);
    static ftl::allocator::BumpPoolBufferStrategy strategy1280(kAllocator, 1280);
    static ftl::allocator::BumpPoolBufferStrategy strategy1536(kAllocator, 1536);
    static ftl::allocator::BumpPoolBufferStrategy strategy1792(kAllocator, 1792);
    static ftl::allocator::BumpPoolBufferStrategy strategy2048(kAllocator, 2048);
    
    static ftl::allocator::BufferAllocator allocator(
        strategy256, strategy512, strategy768, strategy1024,
        strategy1280, strategy1536, strategy1792, strategy2048
    );
    Payload::initialize(allocator);

    // Create and configure the sender socket:
    SocketPtr sender = lo.CreateUdpSocket();
    if (!sender->open(4)) {
        std::cerr << "Failed to open sender socket\n";
        return 1;
    }
    if (!sender->bind(kSenderPort)) {
        std::cerr << "Failed to bind sender to loopback\n";
        return 1;
    }

    // Create and configure the receiver socket:
    SocketPtr receiver = lo.CreateUdpSocket();
    if (!receiver->open(4)) {
        std::cerr << "Failed to open receiver socket\n";
        return 1;
    }
    // Bind the receiver to port kReceiverPort on loopback (INADDR_ANY in our implementation will use loopback)
    if (!receiver->bind(kReceiverPort)) {
        std::cerr << "Failed to bind receiver to port " << kReceiverPort << "\n";
        return 1;
    }
    // Join the multicast group on loopback:
    if (!receiver->join_multicast_group(kMulticastGroup)) {
        std::cerr << "Failed to join multicast group " << kMulticastGroup.ToString().c_str() << "\n";
        return 1;
    }

    Endpoint multiDst{ kMulticastGroup, kReceiverPort };
    Endpoint uniDst{ kLocalAddress,  kReceiverPort };

    std::cout << "Starting send/receive loop (127.0.0.1 → {unicast,multicast} on port " 
              << kReceiverPort << ")\n";

    while (true) {
        // --- Send a multicast packet “Hello multicast” ---
        {
            const char *msg = "Hello multicast";
            size_t      len = std::strlen(msg);
            Payload     p_send(len);
            std::memcpy(p_send.front(), msg, len);

            bool ok = sender->send(std::move(p_send), multiDst);
            if (!ok) {
                std::cerr << "[send] multicast error, errno=" << errno
                          << " (" << std::strerror(errno) << ")\n";
            } else {
                std::cout << "[send] multicast: “" << msg 
                          << "” → " << multiDst.address().ToString().c_str()
                          << ":" << multiDst.port() << "\n";
            }
        }

        // --- Send a unicast packet “Hello unicast” ---
        {
            const char *msg = "Hello unicast";
            size_t      len = std::strlen(msg);
            Payload     p_send(len);
            std::memcpy(p_send.front(), msg, len);

            bool ok = sender->send(std::move(p_send), uniDst);
            if (!ok) {
                std::cerr << "[send] unicast error, errno=" << errno
                          << " (" << std::strerror(errno) << ")\n";
            } else {
                std::cout << "[send] unicast: “" << msg 
                          << "” → " << uniDst.address().ToString().c_str()
                          << ":" << uniDst.port() << "\n";
            }
        }

        // Give the kernel a moment to emit and loop back the packets
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // --- Attempt to receive any pending packets (both multicast and unicast) ---
        {
            Endpoint peer{};
            while (true) {
                Payload  p_recv = receiver->receive(&peer);
                if (!p_recv) {
                    break;  // no more data right now
                }
                std::string received{ reinterpret_cast<char*>(p_recv.front()), p_recv.size() };
                std::cout << "[recv] " 
                          << peer.address().ToString().c_str() << ":" << peer.port()
                          << " → “" << received << "”\n";
            }
        }

        // Wait a bit before sending again
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return 0;
}
