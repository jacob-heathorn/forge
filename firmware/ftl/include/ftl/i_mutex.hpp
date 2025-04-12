// ftl_mutex_interface.hpp
#pragma once

namespace ftl {

// Abstract interface for mutex implementations.
class IMutex {
 public:
  virtual ~IMutex() = default;

  // Blocks until the mutex is acquired.
  virtual void lock() = 0;

  // Tries to acquire the mutex. Returns true on success.
  virtual bool try_lock() = 0;

  // Releases the mutex.
  virtual void unlock() = 0;
};

}  // namespace ftl

#include "ftl/bits/ftl_mutex.hpp"
