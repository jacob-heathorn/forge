#pragma once

#include "ftl/bits/i_mutex.hpp"
#include <mutex>

namespace ftl {

// An implementation of IMutex using std::mutex (for Linux / unit testing)
class mutex : public IMutex {
 public:
  mutex() = default;
  ~mutex() override = default;

  void lock() override {
    mtx_.lock();
  }

  bool try_lock() override {
    return mtx_.try_lock();
  }

  void unlock() override {
    mtx_.unlock();
  }

  mutex(const mutex&) = delete;
  mutex& operator=(const mutex&) = delete;

 private:
  std::mutex mtx_;
};

}  // namespace ftl
