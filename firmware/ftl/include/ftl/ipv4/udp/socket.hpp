#pragma once

#include "stdint.h"

#include "ftl/deleter.hpp"
#include "ftl/ipv4/endpoint.hpp"
#include "ftl/ipv4/udp/payload.hpp"

namespace ftl::ipv4::udp {

class Socket {
public:
  Socket() = default;

  virtual ~Socket() = default;
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;
  Socket(Socket&&) = delete;
  Socket& operator=(Socket&&) = delete;

  virtual bool open(size_t recieve_queue_len = 1) = 0;
  virtual bool is_open() const noexcept = 0;
  virtual bool bind(uint16_t port = 0) = 0;
  virtual bool send(ipv4::udp::Payload payload, const ipv4::Endpoint dest) = 0;
  virtual ipv4::udp::Payload receive(ipv4::Endpoint *const peer) = 0;
  virtual void close() = 0;

  // Joins the given multicast group on the local interface.
  virtual bool join_multicast_group(const ftl::ipv4::Address &group) = 0;

  // Leaves the given multicast group on the local interface.
  virtual bool leave_multicast_group(const ftl::ipv4::Address &group) = 0;
};

using SocketPtr = std::unique_ptr<Socket, DelegatingDeleter<Socket>>;

}
