#pragma once

#include <thread>
#include "ftl/bits/i_thread.hpp"

namespace ftl {

// Cross-platform thread wrapper that uses std::thread underneath.
class thread : public IThread {
 public:
 thread() = default;

  template <typename Callable, typename... Args>
  explicit thread(Callable&& func, Args&&... args)
      : thread_(std::forward<Callable>(func), std::forward<Args>(args)...) {}

  // Rule of 5
  thread(const thread&) = delete;
  thread& operator=(const thread&) = delete;

  thread(thread&& other) noexcept = default;
  thread& operator=(thread&& other) noexcept = default;

  ~thread() override {
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

}  // namespace ftl
