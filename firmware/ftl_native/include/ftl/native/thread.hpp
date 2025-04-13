#pragma once

#include <thread>
#include "ftl/bits/i_thread.hpp"

namespace ftl::native {

// Cross-platform thread wrapper that uses std::thread underneath.
class Thread : public IThread {
 public:
 Thread() = default;

  template <typename Callable, typename... Args>
  explicit Thread(Callable&& func, Args&&... args)
      : thread_(std::forward<Callable>(func), std::forward<Args>(args)...) {}

  // Rule of 5
  Thread(const Thread&) = delete;
  Thread& operator=(const Thread&) = delete;

  Thread(Thread&& other) noexcept = default;
  Thread& operator=(Thread&& other) noexcept = default;

  ~Thread() override {
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
