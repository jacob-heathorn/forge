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

  ftl::ethernet::NativeEthernetInterface lo{Address{"127.0.0.1"}, Mask{"255.255.255.0"}};

  ftl::DataFrame::initialize(kAllocator);

  // Create sender and receiver
  ftl::ipv4::udp::SocketPtr sender = lo.CreateUdpSocket();
  ftl::ipv4::udp::SocketPtr receiver = lo.CreateUdpSocket();

  if (!sender->open(4)) {
      std::cerr << "Failed to open sender socket\n";
      return 1;
  }
  if (!receiver->open(4)) {
      std::cerr << "Failed to open receiver socket\n";
      return 1;
  }

  // Bind receiver to PORT on loopback
  if (!receiver->bind(kReceiverPort)) {
      std::cerr << "Failed to bind receiver to port " << kReceiverPort << "\n";
      return 1;
  }

  Endpoint dst{kLocalAddress, kReceiverPort};

  std::cout << "Starting send/receive loop on 127.0.0.1:" << kReceiverPort << "\n";

  while (true) {
      // 1) Prepare the message
      const char *msg = "hello_udp";
      size_t      len = std::strlen(msg);
      Payload     p_send(len);
      std::memcpy(p_send.data(), msg, len);

      // 2) Send it
      bool ok = sender->send(std::move(p_send), dst);
      if (!ok) {
          std::cerr 
            << "[send] error, return=false, errno=" << errno 
            << " (" << std::strerror(errno) << ")\n";
      } else {
          std::cout 
            << "[send] sent " << len 
            << " bytes → " << dst.address().ToString().c_str()
            << ":" << dst.port() << "\n";
      }

      // 3) Receive it back
      Endpoint peer;
      Payload  p_recv = receiver->receive(&peer);
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
