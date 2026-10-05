#pragma once

#include <cassert>

#include "forge/ftl/bits/lock_guard.hpp"
#include "tx_api.h"

namespace ftl {

inline char kMutexName[] = "FtlMutex";

// The ThreadX mutex, with priority inheritance.
class Mutex {
 public:
  Mutex() {
    while (TX_SUCCESS != tx_mutex_create(&handle_, kMutexName, TX_INHERIT)) {
    }
  }
  ~Mutex() { tx_mutex_delete(&handle_); }
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  void lock() {
    while (TX_SUCCESS != tx_mutex_get(&handle_, TX_WAIT_FOREVER)) {
    }
  }
  bool try_lock() { return tx_mutex_get(&handle_, TX_NO_WAIT) == TX_SUCCESS; }
  void unlock() {
    while (TX_SUCCESS != tx_mutex_put(&handle_)) {
    }
  }

  TX_MUTEX* native_handle() { return &handle_; }

 private:
  TX_MUTEX handle_;
};

static_assert(Lockable<Mutex>);

}  // namespace ftl
