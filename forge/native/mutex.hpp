#pragma once

#include <mutex>

#include "forge/ftl/bits/lock_guard.hpp"

namespace ftl {

// The host mutex, over std::mutex.
class Mutex {
 public:
  Mutex() = default;
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  void lock() { mutex_.lock(); }
  bool try_lock() { return mutex_.try_lock(); }
  void unlock() { mutex_.unlock(); }

 private:
  std::mutex mutex_;
};

static_assert(Lockable<Mutex>);

}  // namespace ftl
