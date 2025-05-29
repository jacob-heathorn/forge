// Filename: native_udp_socket_test.cpp

#include <gtest/gtest.h>
#include "ftl/native_udp_socket.hpp"
#include "ftl/ipv4/endpoint.hpp"
#include "ftl/ipv4/udp/payload.hpp"

#include <cstring>

using ftl::ipv4::Address;
using ftl::ipv4::Endpoint;
using ftl::ipv4::udp::NativeUdpSocket;
using ftl::ipv4::udp::Payload;

class NativeUdpSocketTest : public ::testing::Test {
protected:
  static constexpr size_t POOL_MEMORY_SIZE = 16 * 1024;
  uint8_t*           buffer_;
  ftl::BumpAllocator* allocator_;
  NativeUdpSocket sender_;
  NativeUdpSocket receiver_;
  
  
  void SetUp() override {
    buffer_    = new uint8_t[POOL_MEMORY_SIZE];
    allocator_ = new ftl::BumpAllocator(buffer_, POOL_MEMORY_SIZE);
    ftl::DataFrame::initialize(*allocator_);
    ASSERT_TRUE(sender_.open(4));
    ASSERT_TRUE(receiver_.open(4));
  }

  void TearDown() override {
    sender_.close();
    receiver_.close();
    delete allocator_;
    delete[] buffer_;
  }
};

// Test unicast send/receive on loopback
TEST_F(NativeUdpSocketTest, UnicastSendReceive) {
    constexpr uint16_t PORT = 54321;
    ASSERT_TRUE(receiver_.bind(PORT));

    // Prepare payload (Payload(size) auto-sets .size())
    const char *msg = "hello_unicast";
    (void)msg;
    Payload p_send(std::strlen(msg));
    std::memcpy(p_send.data(), msg, std::strlen(msg));

    // Send to loopback 127.0.0.1
    Endpoint dst(Address{"127.0.0.1"}, PORT);
    ASSERT_TRUE(sender_.send(std::move(p_send), dst));

    // Receive
    Endpoint peer{};
    Payload p_recv = receiver_.receive(&peer);
    ASSERT_TRUE(p_recv);
    // std::string received(reinterpret_cast<char*>(p_recv.data()), p_recv.size());
    // EXPECT_EQ(received, msg);

    // // Peer addr must match exactly an Address
    // EXPECT_EQ(peer.address(), Address{0x7F000001u});
    // EXPECT_EQ(peer.port(), PORT);
}

// // Test multicast send/receive on loopback
// TEST_F(NativeUdpSocketTest, MulticastSendReceive) {
//     constexpr uint16_t PORT = 12345;
//     Address group{0xEF00002Au};  // 239.0.0.42

//     ASSERT_TRUE(receiver_.bind(PORT));
//     ASSERT_TRUE(receiver_.join_multicast_group(group));

//     const char *msg = "hello_multicast";
//     Payload p_send(std::strlen(msg));
//     std::memcpy(p_send.data(), msg, std::strlen(msg));

//     Endpoint dst(group, PORT);
//     ASSERT_TRUE(sender_.send(std::move(p_send), dst));

//     Endpoint peer{};
//     Payload p_recv = receiver_.receive(&peer);
//     ASSERT_EQ(p_recv.size(), std::strlen(msg));
//     std::string received(reinterpret_cast<char*>(p_recv.data()), p_recv.size());
//     EXPECT_EQ(received, msg);

//     // For multicast we only reliably check the port
//     EXPECT_EQ(peer.port(), PORT);

//     ASSERT_TRUE(receiver_.leave_multicast_group(group));
// }
