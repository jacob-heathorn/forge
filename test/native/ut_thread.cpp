#include "gtest/gtest.h"

#include "ftl/native/thread.hpp"   // ftl::native::Thread declaration (std::thread–based)
#include "ftl/mutex.hpp"    // ftl::mutex declaration (std::mutex–based)

#include <atomic>
#include <chrono>
#include <thread>  // For std::this_thread::sleep_for

// Anonymous namespace for unit tests.
namespace {

//
// Test 1: Verify that a thread executing a lambda runs and sets a flag.
//
TEST(FtlThreadTest, ThreadExecutesLambda) {
  std::atomic<bool> ran{false};

  {
    ftl::native::Thread t([&]() {
      ran.store(true);
    });
    t.join();
  }
  EXPECT_TRUE(ran.load());
}

//
// Test 2: Verify that a thread can be detached and eventually runs.
// Note: Detach means the thread is not joinable; we use a sleep in main thread to allow work.
//
TEST(FtlThreadTest, ThreadCanBeDetached) {
  std::atomic<bool> ran{false};

  {
    ftl::native::Thread t([&]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      ran.store(true);
    });
    t.detach();
    // Give the thread time to run.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  EXPECT_TRUE(ran.load());
}

//
// Test 3: Verify that a shared resource is correctly protected by ftl::mutex and ftl::lock_guard
//
TEST(FtlThreadTest, MutexGuardsSharedResource) {
  ftl::mutex mutex;
  int counter = 0;

  auto increment = [&]() {
    for (int i = 0; i < 10000; ++i) {
      ftl::lock_guard<ftl::mutex> guard(mutex);
      ++counter;
    }
  };

  ftl::native::Thread t1(increment);
  ftl::native::Thread t2(increment);

  t1.join();
  t2.join();

  EXPECT_EQ(counter, 20000);
}

//
// Test 4: Verify that joinable() reflects thread state correctly.
//
TEST(FtlThreadTest, IsJoinableReportsCorrectly) {
  ftl::native::Thread t([] {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  });

  EXPECT_TRUE(t.joinable());
  t.join();
  EXPECT_FALSE(t.joinable());
}

//
// Test 5: Verify using a class member function with an argument as thread work.
//
TEST(FtlThreadTest, ClassMethodWithArgument) {
  // A simple test class with a member function that takes an int parameter.
  class TestClass {
   public:
    TestClass() : result(0) {}
    void doWork(int value) { result = value; }
    int result;
  };

  TestClass obj;
  // Create a thread that calls the member function.
  // The thread constructor should forward the arguments appropriately.
  ftl::native::Thread t(&TestClass::doWork, &obj, 456);
  t.join();

  EXPECT_EQ(obj.result, 456);
}

}  // namespace
