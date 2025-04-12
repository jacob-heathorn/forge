// bits/ftl_mutex.cpp
#include "ftl/mutex.hpp"

namespace ftl {

// Tag types for lock_guard
struct defer_lock_t {};
struct try_to_lock_t {};
struct adopt_lock_t {};

inline constexpr defer_lock_t defer_lock{};
inline constexpr try_to_lock_t try_to_lock{};
inline constexpr adopt_lock_t adopt_lock{};

// Simplified lock_guard
template <class M>
class lock_guard {
private:
  M& mtx;
  bool owns_;

public:
  // Lock immediately
  explicit lock_guard(M& m) : mtx(m), owns_(true) {
    mtx.lock();
  }

  // Assume already locked
  lock_guard(M& m, adopt_lock_t) : mtx(m), owns_(true) {}

  // Defer locking (user manually calls lock())
  lock_guard(M& m, defer_lock_t) : mtx(m), owns_(false) {}

  // Try to lock (may fail silently)
  lock_guard(M& m, try_to_lock_t) : mtx(m), owns_(m.try_lock()) {}

  ~lock_guard() {
    if (owns_) {
      mtx.unlock();
    }
  }

  // Optional: expose ownership for try_to_lock cases
  bool owns_lock() const { return owns_; }

  lock_guard(const lock_guard&) = delete;
  lock_guard& operator=(const lock_guard&) = delete;
};

} // namespace ftl
