#pragma once

namespace ftl {

// Tag types for LockGuard
struct defer_lock_t {};
struct try_to_lock_t {};
struct adopt_lock_t {};

inline constexpr defer_lock_t defer_lock{};
inline constexpr try_to_lock_t try_to_lock{};
inline constexpr adopt_lock_t adopt_lock{};

template <class M>
class LockGuard {
private:
  M& mtx;     // Reference to the mutex
  bool owns_; // Indicates whether this guard owns the lock

public:
  // Locks the mutex immediately upon construction
  explicit LockGuard(M& m) : mtx(m), owns_(true) {
    mtx.lock();
  }

  // Assumes the mutex is already locked
  LockGuard(M& m, adopt_lock_t) : mtx(m), owns_(true) {}

  // Defers locking; user must call lock manually
  LockGuard(M& m, defer_lock_t) : mtx(m), owns_(false) {}

  // Tries to lock the mutex; may fail
  LockGuard(M& m, try_to_lock_t) : mtx(m), owns_(m.try_lock()) {}

  // Unlocks the mutex if owned
  ~LockGuard() {
    if (owns_) {
      mtx.unlock();
    }
  }

  // Returns whether the lock is currently owned
  bool owns_lock() const { return owns_; }

  // Non-copyable and non-movable
  LockGuard(const LockGuard&) = delete;
  LockGuard& operator=(const LockGuard&) = delete;
  LockGuard(LockGuard&&) = delete;
  LockGuard& operator=(LockGuard&&) = delete;
};


} // namespace ftl
