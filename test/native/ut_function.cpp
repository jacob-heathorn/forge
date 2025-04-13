#include "gtest/gtest.h"
#include "ftl/functional.hpp"  // Adjust the include path to your ftl::function header

namespace {

// A free function that takes an int and returns an int.
int FreeFunction(int x) {
  return x * 3;
}

// A simple test class with a member function.
class TestClass {
 public:
  explicit TestClass(int v) : value(v) {}
  
  // Member function that multiplies the object's value with a factor.
  int multiply(int factor) {
    return value * factor;
  }
  
 private:
  int value;
};

// Test ftl::function with a free function.
TEST(FtlFunctionTest, FreeFunctionCall) {
  // Create an ftl::function that stores a callable with signature int(int)
  ftl::function<int(int)> func = FreeFunction;
  EXPECT_EQ(func(4), 12);  // 4 * 3 = 12
}

// Test ftl::function with a member function.
// Since ftl::function only accepts callables that match the signature,
// we bind the member function to an instance via a lambda.
TEST(FtlFunctionTest, MemberFunctionCall) {
  TestClass obj(5);
  
  // Use a lambda to capture the object reference and call its member function.
  ftl::function<int(int)> func = [&obj](int factor) -> int {
    return obj.multiply(factor);
  };
  
  EXPECT_EQ(func(3), 15);  // 5 * 3 = 15
}

// Test ftl::function with a capturing lambda.
TEST(FtlFunctionTest, CapturingLambdaCall) {
  int offset = 7;
  ftl::function<int(int)> func = [offset](int x) -> int {
    return x + offset;
  };
  EXPECT_EQ(func(5), 12);  // 5 + 7 = 12
}

}  // namespace
