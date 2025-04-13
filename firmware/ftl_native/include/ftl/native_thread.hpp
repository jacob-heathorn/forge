#pragma once

#include <thread>
#include "ftl/bits/i_thread.hpp"

namespace ftl {

class NativeThread : public IThread {
 public:
 NativeThread() = default;

  template <typename Callable, typename... Args>
  explicit NativeThread(Callable&& func, Args&&... args)
      : thread_(std::forward<Callable>(func), std::forward<Args>(args)...) {}

  // Rule of 5
  NativeThread(const NativeThread&) = delete;
  NativeThread& operator=(const NativeThread&) = delete;

  NativeThread(NativeThread&& other) noexcept = default;
  NativeThread& operator=(NativeThread&& other) noexcept = default;

  ~NativeThread() override {
    if (thread_.joinable()) {
      thread_.detach();  // Avoid std::terminate
    }
  }

  void join() override {
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  void detach() override {
    if (thread_.joinable()) {
      thread_.detach();
    }
  }

  bool joinable() const override {
    return thread_.joinable();
  }

 private:
  std::thread thread_;
};

}
