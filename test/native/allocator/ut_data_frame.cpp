#include <gtest/gtest.h>
#include "ftl/allocator/data_frame.hpp"
#include "ftl/allocator/buffer_allocator.hpp"
#include "ftl/allocator/malloc_buffer_strategy.hpp"
#include <cstring>

using namespace ftl::allocator;

// Test DataFrame class inheriting from DataFrame template
class TestFrame : public DataFrame<TestFrame> {
public:
  explicit TestFrame(std::size_t size)
    : DataFrame(size) {}

  TestFrame() = default;

  // Movable but not copyable
  TestFrame(TestFrame&&) noexcept = default;
  TestFrame& operator=(TestFrame&&) noexcept = default;
  TestFrame(const TestFrame&) = delete;
  TestFrame& operator=(const TestFrame&) = delete;
  ~TestFrame() = default;
};

class DataFrameTest : public ::testing::Test {
protected:
  void SetUp() override {
    strategy_ = std::make_unique<MallocBufferStrategy>();
    allocator_ = std::make_unique<BufferAllocator>(*strategy_);
    TestFrame::initialize(*allocator_);
  }

  void TearDown() override {
    // Clean up allocator
    allocator_.reset();
    strategy_.reset();
  }

  std::unique_ptr<MallocBufferStrategy> strategy_;
  std::unique_ptr<BufferAllocator> allocator_;
};

TEST_F(DataFrameTest, DefaultConstruction) {
  TestFrame frame;
  EXPECT_FALSE(frame);
  EXPECT_EQ(frame.front(), nullptr);
  EXPECT_EQ(frame.size(), 0u);
}

TEST_F(DataFrameTest, SizeConstruction) {
  const std::size_t test_size = 100;
  TestFrame frame(test_size);

  EXPECT_TRUE(frame);
  EXPECT_NE(frame.front(), nullptr);
  EXPECT_EQ(frame.size(), test_size);
}

TEST_F(DataFrameTest, MoveConstruction) {
  const std::size_t test_size = 50;
  TestFrame frame1(test_size);
  ASSERT_TRUE(frame1);

  // Store pointer for comparison
  auto* original_ptr = frame1.front();

  // Move construct
  TestFrame frame2(std::move(frame1));

  // Check frame2 has the data
  EXPECT_TRUE(frame2);
  EXPECT_EQ(frame2.front(), original_ptr);
  EXPECT_EQ(frame2.size(), test_size);

  // Check frame1 is empty after move
  EXPECT_FALSE(frame1);
  EXPECT_EQ(frame1.front(), nullptr);
  EXPECT_EQ(frame1.size(), 0u);
}

TEST_F(DataFrameTest, MoveAssignment) {
  const std::size_t size1 = 30;
  const std::size_t size2 = 60;

  TestFrame frame1(size1);
  TestFrame frame2(size2);

  ASSERT_TRUE(frame1);
  ASSERT_TRUE(frame2);

  auto* ptr2 = frame2.front();

  // Move assign
  frame1 = std::move(frame2);

  // Check frame1 has frame2's data
  EXPECT_TRUE(frame1);
  EXPECT_EQ(frame1.front(), ptr2);
  EXPECT_EQ(frame1.size(), size2);

  // Check frame2 is empty
  EXPECT_FALSE(frame2);
  EXPECT_EQ(frame2.front(), nullptr);
  EXPECT_EQ(frame2.size(), 0u);
}

TEST_F(DataFrameTest, DataReadWrite) {
  const std::size_t test_size = 256;
  TestFrame frame(test_size);

  ASSERT_TRUE(frame);
  ASSERT_NE(frame.front(), nullptr);

  // Write pattern to buffer
  for (std::size_t i = 0; i < test_size; ++i) {
    frame.front()[i] = static_cast<uint8_t>(i % 256);
  }

  // Read back and verify
  for (std::size_t i = 0; i < test_size; ++i) {
    EXPECT_EQ(frame.front()[i], static_cast<uint8_t>(i % 256));
  }
}

TEST_F(DataFrameTest, StringView) {
  const char test_string[] = "Hello, DataFrame!";
  const std::size_t str_len = sizeof(test_string) - 1; // Exclude null terminator

  TestFrame frame(str_len);
  ASSERT_TRUE(frame);

  // Copy string to frame
  std::memcpy(frame.front(), test_string, str_len);

  // Get string_view and verify
  auto sv = frame.string_view();
  EXPECT_EQ(sv.size(), str_len);
  EXPECT_EQ(sv, test_string);
}

TEST_F(DataFrameTest, RawBufferAccess) {
  const std::size_t test_size = 128;
  TestFrame frame(test_size);

  ASSERT_TRUE(frame);

  // Test raw_buffer() returns non-null
  EXPECT_NE(frame.raw_buffer(), nullptr);

  // Const version
  const TestFrame& const_frame = frame;
  EXPECT_NE(const_frame.raw_buffer(), nullptr);
}

TEST_F(DataFrameTest, LargeAllocation) {
  const std::size_t large_size = 65536; // 64KB
  TestFrame frame(large_size);

  EXPECT_TRUE(frame);
  EXPECT_NE(frame.front(), nullptr);
  EXPECT_EQ(frame.size(), large_size);

  // Write and verify first and last bytes
  frame.front()[0] = 0xAA;
  frame.front()[large_size - 1] = 0xBB;

  EXPECT_EQ(frame.front()[0], 0xAA);
  EXPECT_EQ(frame.front()[large_size - 1], 0xBB);
}

TEST_F(DataFrameTest, MultipleFrameTypes) {
  // Define another frame type
  class AnotherFrame : public DataFrame<AnotherFrame> {
  public:
    explicit AnotherFrame(std::size_t size) : DataFrame(size) {}
    AnotherFrame() = default;
    AnotherFrame(AnotherFrame&&) noexcept = default;
    AnotherFrame& operator=(AnotherFrame&&) noexcept = default;
    AnotherFrame(const AnotherFrame&) = delete;
    AnotherFrame& operator=(const AnotherFrame&) = delete;
    ~AnotherFrame() = default;
  };

  // Initialize AnotherFrame with same allocator
  AnotherFrame::initialize(*allocator_);

  // Create instances of both types
  TestFrame test_frame(100);
  AnotherFrame another_frame(200);

  EXPECT_TRUE(test_frame);
  EXPECT_TRUE(another_frame);
  EXPECT_EQ(test_frame.size(), 100u);
  EXPECT_EQ(another_frame.size(), 200u);

  // Verify they have separate static allocators (each type has its own)
  EXPECT_NE(test_frame.raw_buffer(), another_frame.raw_buffer());
}

TEST_F(DataFrameTest, SizeAssertion) {
  // Verify the static_assert about size
  EXPECT_EQ(sizeof(TestFrame), 3 * sizeof(void*));
}