#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>

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

    ftl::DataFrame::initialize(kAllocator);

    constexpr uint16_t PORT = 54321;
    const Address       LOCAL_ADDR{"127.0.0.1"};

    // Create sender and receiver
    NativeUdpSocket sender;
    NativeUdpSocket receiver;

    if (!sender.open(4)) {
        std::cerr << "Failed to open sender socket\n";
        return 1;
    }
    if (!receiver.open(4)) {
        std::cerr << "Failed to open receiver socket\n";
        return 1;
    }

    // Bind receiver to PORT on loopback
    if (!receiver.bind(PORT)) {
        std::cerr << "Failed to bind receiver to port " << PORT << "\n";
        return 1;
    }

    Endpoint dst{LOCAL_ADDR, PORT};

    std::cout << "Starting send/receive loop on 127.0.0.1:" << PORT << "\n";

    while (true) {
        // 1) Prepare the message
        const char *msg = "hello_udp";
        size_t      len = std::strlen(msg);
        Payload     p_send(len);
        std::memcpy(p_send.data(), msg, len);

        // 2) Send it
        if (!sender.send(std::move(p_send), dst)) {
            std::cerr << "[send] error\n";
        }

        // 3) Receive it back
        Endpoint peer;
        Payload  p_recv = receiver.receive(&peer);
        if (p_recv) {
            std::string received{reinterpret_cast<char*>(p_recv.data()), p_recv.size()};
            std::cout << "[recv] " 
                      << peer.address().ToString().c_str() << ":" << peer.port()
                      << " → " << received << "\n";
        } else {
            std::cout << "[recv] no data\n";
        }

        // 4) Wait 100 ms before next iteration
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}
