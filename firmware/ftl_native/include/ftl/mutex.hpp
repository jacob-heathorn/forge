#pragma once

#include "ftl/i_mutex.hpp"
#include <mutex>

namespace ftl {

// An implementation of IMutex using std::mutex (for Linux / unit testing)
class Mutex : public IMutex {
 public:
 Mutex() = default;
  ~Mutex() override = default;

  void lock() override {
    mtx_.lock();
  }

  bool try_lock() override {
    return mtx_.try_lock();
  }

  void unlock() override {
    mtx_.unlock();
  }

  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

 private:
  std::mutex mtx_;
};

}  // namespace ftl
