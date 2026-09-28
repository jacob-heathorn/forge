#pragma once

#include <concepts>

namespace ftl {

// What a mutex must provide; "forge/ftl/mutex.hpp" is served by a platform backend that satisfies it.
template <typename M>
concept Lockable = requires(M m) {
  m.lock();
  m.unlock();
  { m.try_lock() } -> std::same_as<bool>;
};

}  // namespace ftl
