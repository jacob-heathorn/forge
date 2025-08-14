// native_udp_socket_test.cpp

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <cstring>
#include <vector>
#include <memory>

#include "ftl/native_ethernet_interface.hpp"
#include "ftl/ipv4/endpoint.hpp"
#include "ftl/ipv4/mask.hpp"
#include "ftl/ipv4/udp/socket.hpp"
#include "ftl/ipv4/udp/payload.hpp"
#include "ftl/allocator/bump_allocator.hpp"
#include "ftl/allocator/bump_pool_buffer_strategy.hpp"
#include "ftl/allocator/buffer_allocator.hpp"

using ftl::ethernet::NativeEthernetInterface;
using ftl::ipv4::Address;
using ftl::ipv4::Mask;
using ftl::ipv4::Endpoint;
using ftl::ipv4::udp::SocketPtr;
using ftl::ipv4::udp::Payload;

class NativeUdpSocketTest : public ::testing::Test {
  protected:
    static constexpr size_t POOL_MEMORY_SIZE = 16 * 1024;
    uint8_t*              buffer_   = nullptr;
    ftl::BumpAllocator*   allocator_= nullptr;
    std::vector<std::unique_ptr<ftl::allocator::BumpPoolBufferStrategy>> strategies_;
    ftl::allocator::BufferAllocator* buffer_allocator_ = nullptr;
    NativeEthernetInterface* lo_     = nullptr;
    SocketPtr             sender_;
    SocketPtr             receiver_;

    void SetUp() override {
        // bump allocator + Payload
        buffer_    = new uint8_t[POOL_MEMORY_SIZE];
        allocator_ = new ftl::BumpAllocator(buffer_, POOL_MEMORY_SIZE);
        
        // Initialize Payload allocator with bump pool strategies
        strategies_.reserve(8);
        strategies_.push_back(std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*allocator_, 256));
        strategies_.push_back(std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*allocator_, 512));
        strategies_.push_back(std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*allocator_, 768));
        strategies_.push_back(std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*allocator_, 1024));
        strategies_.push_back(std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*allocator_, 1280));
        strategies_.push_back(std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*allocator_, 1536));
        strategies_.push_back(std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*allocator_, 1792));
        strategies_.push_back(std::make_unique<ftl::allocator::BumpPoolBufferStrategy>(*allocator_, 2048));
        
        buffer_allocator_ = new ftl::allocator::BufferAllocator(
            *strategies_[0], *strategies_[1], *strategies_[2], *strategies_[3],
            *strategies_[4], *strategies_[5], *strategies_[6], *strategies_[7]
        );
        Payload::initialize(*buffer_allocator_);

        // construct the interface here
        lo_ = new NativeEthernetInterface(
            Address{"127.0.0.1"},
            Mask   {"255.255.255.0"}
        );

        // create sockets
        sender_   = std::move(lo_->CreateUdpSocket());
        receiver_ = std::move(lo_->CreateUdpSocket());

        ASSERT_TRUE(sender_->open(4));
        ASSERT_TRUE(receiver_->open(4));
    }

    void TearDown() override {
        sender_->close();
        receiver_->close();
        delete lo_;
        delete buffer_allocator_;
        strategies_.clear();
        delete allocator_;
        delete[] buffer_;
    }
};
  

// Test unicast send/receive on loopback
TEST_F(NativeUdpSocketTest, UnicastSendReceive) {
    constexpr uint16_t kSenderPort   = 5555;
    constexpr uint16_t kReceiverPort = 5556;

    ASSERT_TRUE(sender_->bind(kSenderPort));
    ASSERT_TRUE(receiver_->bind(kReceiverPort));

    const char* msg = "hello_unicast";
    Payload p_send(std::strlen(msg));
    std::memcpy(p_send.front(), msg, std::strlen(msg));

    Endpoint dst{ Address{"127.0.0.1"}, kReceiverPort };
    ASSERT_TRUE(sender_->send(std::move(p_send), dst));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Endpoint peer{};
    Payload  p_recv = receiver_->receive(&peer);
    ASSERT_TRUE(p_recv);
    std::string received{ reinterpret_cast<char*>(p_recv.front()), p_recv.size() };
    EXPECT_EQ(received, msg);
    EXPECT_EQ(peer.address(), Address{"127.0.0.1"});
    EXPECT_EQ(peer.port(),    kSenderPort);
}

// Test multicast send/receive on loopback
TEST_F(NativeUdpSocketTest, MulticastSendReceive) {
    constexpr uint16_t kSenderPort   = 5555;
    constexpr uint16_t kReceiverPort = 5556;
    Address group{ 239, 0, 0, 42 };  // 239.0.0.42

    ASSERT_TRUE(sender_->bind(kSenderPort));
    ASSERT_TRUE(receiver_->bind(kReceiverPort));
    ASSERT_TRUE(receiver_->join_multicast_group(group));

    const char* msg = "hello_multicast";
    Payload p_send(std::strlen(msg));
    std::memcpy(p_send.front(), msg, std::strlen(msg));

    Endpoint dst{ group, kReceiverPort };
    ASSERT_TRUE(sender_->send(std::move(p_send), dst));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    Endpoint peer{};
    Payload  p_recv = receiver_->receive(&peer);
    ASSERT_TRUE(p_recv);
    std::string received{ reinterpret_cast<char*>(p_recv.front()), p_recv.size() };
    EXPECT_EQ(received, msg);

    // For multicast we only reliably check the source port
    EXPECT_EQ(peer.port(), kSenderPort);

    ASSERT_TRUE(receiver_->leave_multicast_group(group));
}
