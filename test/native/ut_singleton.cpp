#include "gtest/gtest.h"
#include "ftl/singleton.hpp"
#include <cassert>

// Define a simple test class that inherits from ftl::Singleton.
namespace {
class TestSingleton : public ftl::Singleton<TestSingleton> {
    // Allow the singleton base to access TestSingleton's constructor.
    friend class ftl::Singleton<TestSingleton>;
public:
    int value;

    // A non-default constructor for testing.
    TestSingleton(int v) : value(v) { }
};
}  // anonymous namespace

// Death test: Calling instance() before create() should assert.
// Death tests are slow because they fork a subprocess
// Uncomment to enable, or use --gtest_also_run_disabled_tests to run
TEST(SingletonDeathTest, DISABLED_InstanceWithoutCreate) {
    // Ensure a clean state.
    TestSingleton::destroy();
    EXPECT_DEATH({
        // This should trigger the assertion "Singleton not created! Call create() first."
        TestSingleton::instance();
    }, "Singleton not created");
}

// Test: Create the singleton and then access its value.
TEST(SingletonTest, CreateAndInstance) {
    // Start with a clean state.
    TestSingleton::destroy();
    // Create the singleton with a value.
    TestSingleton::create(42);
    // Check that the instance() returns the object with the correct value.
    EXPECT_EQ(TestSingleton::instance().value, 42);
}

// Death test: Calling create() twice should trigger an assertion.
// Death tests are slow because they fork a subprocess
// Uncomment to enable, or use --gtest_also_run_disabled_tests to run
TEST(SingletonDeathTest, DISABLED_CreateTwice) {
    // Clean state.
    TestSingleton::destroy();
    TestSingleton::create(100);
    // Attempting to call create() a second time should assert.
    EXPECT_DEATH({
        TestSingleton::create(200);
    }, "Singleton already created");
}

// Test: Destroy the singleton and then verify that instance() asserts.
// Death tests are slow because they fork a subprocess
// Uncomment to enable, or use --gtest_also_run_disabled_tests to run
TEST(SingletonTest, DISABLED_DestroySingleton) {
    // Ensure the singleton is created.
    TestSingleton::destroy();
    TestSingleton::create(55);
    // Now destroy it.
    TestSingleton::destroy();
    // Calling instance() now should trigger an assertion.
    EXPECT_DEATH({
        TestSingleton::instance();
    }, "Singleton not created");
}
