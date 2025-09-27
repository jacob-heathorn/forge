#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <type_traits>

#include "etl/string.h"
#include "ftl/buffer.hpp"
#include "ftl/allocator/buffer_allocator.hpp"
#include "ftl/allocator/data_frame.hpp"
#include "ftl/allocator/strategy.hpp"
#include "ftl/memory.hpp"

namespace ftl::ipv4::udp {

// Provides an interface to an ethernet data frame to access UDP payload data.
class Payload : public ftl::allocator::DataFrame<Payload> {
public:
  // Construct with payload size
  explicit Payload(std::size_t size)
    : DataFrame(size) {
  }

  Payload() = default;

  // Movable but not copyable
  Payload(Payload&&) noexcept = default;
  Payload& operator=(Payload&&) noexcept = default;
  Payload(const Payload&) = delete;
  Payload& operator=(const Payload&) = delete;
  ~Payload() = default;
};

// Payload inherits from DataFrame: unique_ptr (2 pointers) + vtable pointer from Deleter
static_assert(sizeof(Payload) == 3 * sizeof(void*));

}
