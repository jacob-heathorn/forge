#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

#include "ftl/thread.hpp"
#include "ftl/mutex.hpp"

namespace {

TEST(FtlThreadTest, ThreadExecutesLambda) {
  std::atomic<bool> ran{false};

  {
    ftl::thread thread([&]() {
      ran = true;
    });
    thread.join();
  }

  EXPECT_TRUE(ran.load());
}

TEST(FtlThreadTest, ThreadCanBeDetached) {
  std::atomic<bool> ran{false};

  {
    ftl::thread thread([&]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      ran = true;
    });
    thread.detach();

    // Give the thread some time to run after main thread exits scope
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  EXPECT_TRUE(ran.load());
}

TEST(FtlThreadTest, MutexGuardsSharedResource) {
  ftl::mutex mutex;
  int counter = 0;

  auto increment = [&]() {
    for (int i = 0; i < 10000; ++i) {
      ftl::lock_guard<ftl::mutex> guard(mutex);
      ++counter;
    }
  };

  ftl::thread t1(increment);
  ftl::thread t2(increment);

  t1.join();
  t2.join();

  EXPECT_EQ(counter, 20000);
}

TEST(FtlThreadTest, IsJoinableReportsCorrectly) {
  ftl::thread thread([] {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  });

  EXPECT_TRUE(thread.joinable());

  thread.join();

  EXPECT_FALSE(thread.joinable());  // joinable() is false after join
}

}  // namespace
