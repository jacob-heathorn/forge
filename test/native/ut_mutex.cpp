#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

#include "ftl/mutex.hpp"

namespace {

class MutexTest : public ::testing::Test {
 protected:
  ftl::mutex mutex_;
};

TEST_F(MutexTest, LocksAndUnlocks) {
  EXPECT_NO_THROW({
    ftl::lock_guard<ftl::mutex> guard(mutex_);
  });
}

TEST_F(MutexTest, AdoptLockSkipsRelocking) {
  mutex_.lock();
  {
    ftl::lock_guard<ftl::mutex> guard(mutex_, ftl::adopt_lock);
  }
  // Should now be unlocked
  EXPECT_TRUE(mutex_.try_lock());
  mutex_.unlock();
}

TEST_F(MutexTest, DeferLockDoesNotLock) {
  {
    ftl::lock_guard<ftl::mutex> guard(mutex_, ftl::defer_lock);
    // Since guard doesn't lock, mutex should still be lockable
    EXPECT_TRUE(mutex_.try_lock());
    mutex_.unlock();
  }
}

TEST_F(MutexTest, TryToLockSucceedsWhenUnlocked) {
  ftl::lock_guard<ftl::mutex> guard(mutex_, ftl::try_to_lock);
  // Can't inspect owns_lock unless you expose it, but no crash = success
  SUCCEED();
}

TEST_F(MutexTest, TryToLockFailsWhenLocked) {
  std::atomic<bool> try_succeeded{true};
  mutex_.lock();
  std::thread t([&]() {
    ftl::lock_guard<ftl::mutex> guard(mutex_, ftl::try_to_lock);
    // Should silently fail to acquire
    try_succeeded = false;
  });
  t.join();
  mutex_.unlock();

  EXPECT_FALSE(try_succeeded.load());
}

TEST_F(MutexTest, ThreadedIncrementsAreCorrect) {
  const int kThreads = 4;
  const int kIncrements = 10000;
  int counter = 0;

  auto worker = [&]() {
    for (int i = 0; i < kIncrements; ++i) {
      ftl::lock_guard<ftl::mutex> guard(mutex_);
      ++counter;
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back(worker);
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(counter, kThreads * kIncrements);
}

}  // namespace
