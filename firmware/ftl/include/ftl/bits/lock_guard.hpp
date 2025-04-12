#pragma once

namespace ftl {

// Tag types for lock_guard
struct defer_lock_t {};
struct try_to_lock_t {};
struct adopt_lock_t {};

inline constexpr defer_lock_t defer_lock{};
inline constexpr try_to_lock_t try_to_lock{};
inline constexpr adopt_lock_t adopt_lock{};

template <class M>
class lock_guard {
private:
  M& mtx;     // Reference to the mutex
  bool owns_; // Indicates whether this guard owns the lock

public:
  // Locks the mutex immediately upon construction
  explicit lock_guard(M& m) : mtx(m), owns_(true) {
    mtx.lock();
  }

  // Assumes the mutex is already locked
  lock_guard(M& m, adopt_lock_t) : mtx(m), owns_(true) {}

  // Defers locking; user must call lock manually
  lock_guard(M& m, defer_lock_t) : mtx(m), owns_(false) {}

  // Tries to lock the mutex; may fail
  lock_guard(M& m, try_to_lock_t) : mtx(m), owns_(m.try_lock()) {}

  // Unlocks the mutex if owned
  ~lock_guard() {
    if (owns_) {
      mtx.unlock();
    }
  }

  // Returns whether the lock is currently owned
  bool owns_lock() const { return owns_; }

  // Non-copyable and non-movable
  lock_guard(const lock_guard&) = delete;
  lock_guard& operator=(const lock_guard&) = delete;
  lock_guard(lock_guard&&) = delete;
  lock_guard& operator=(lock_guard&&) = delete;
};


} // namespace ftl
