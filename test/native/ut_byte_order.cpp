#include "gtest/gtest.h"
#include "ftl/byte_order.hpp"
#include <cstring>

class ByteOrderTest : public ::testing::Test {
protected:
    uint8_t buffer[8];

    void SetUp() override {
        std::memset(buffer, 0, sizeof(buffer));
    }
};

// Test uint16_t little-endian operations
TEST_F(ByteOrderTest, WriteReadU16LE) {
    uint16_t original = 0x1234;
    
    ftl::WriteU16LE(buffer, original);
    
    // Verify byte order in buffer
    EXPECT_EQ(buffer[0], 0x34);
    EXPECT_EQ(buffer[1], 0x12);
    
    // Verify round-trip
    uint16_t result = ftl::ReadU16LE(buffer);
    EXPECT_EQ(result, original);
}

// Test uint16_t big-endian operations
TEST_F(ByteOrderTest, WriteReadU16BE) {
    uint16_t original = 0x1234;
    
    ftl::WriteU16BE(buffer, original);
    
    // Verify byte order in buffer
    EXPECT_EQ(buffer[0], 0x12);
    EXPECT_EQ(buffer[1], 0x34);
    
    // Verify round-trip
    uint16_t result = ftl::ReadU16BE(buffer);
    EXPECT_EQ(result, original);
}

// Test uint32_t little-endian operations
TEST_F(ByteOrderTest, WriteReadU32LE) {
    uint32_t original = 0x12345678;
    
    ftl::WriteU32LE(buffer, original);
    
    // Verify byte order in buffer
    EXPECT_EQ(buffer[0], 0x78);
    EXPECT_EQ(buffer[1], 0x56);
    EXPECT_EQ(buffer[2], 0x34);
    EXPECT_EQ(buffer[3], 0x12);
    
    // Verify round-trip
    uint32_t result = ftl::ReadU32LE(buffer);
    EXPECT_EQ(result, original);
}

// Test uint32_t big-endian operations
TEST_F(ByteOrderTest, WriteReadU32BE) {
    uint32_t original = 0x12345678;
    
    ftl::WriteU32BE(buffer, original);
    
    // Verify byte order in buffer
    EXPECT_EQ(buffer[0], 0x12);
    EXPECT_EQ(buffer[1], 0x34);
    EXPECT_EQ(buffer[2], 0x56);
    EXPECT_EQ(buffer[3], 0x78);
    
    // Verify round-trip
    uint32_t result = ftl::ReadU32BE(buffer);
    EXPECT_EQ(result, original);
}

// Test uint64_t little-endian operations
TEST_F(ByteOrderTest, WriteReadU64LE) {
    uint64_t original = 0x123456789ABCDEF0;
    
    ftl::WriteU64LE(buffer, original);
    
    // Verify byte order in buffer
    EXPECT_EQ(buffer[0], 0xF0);
    EXPECT_EQ(buffer[1], 0xDE);
    EXPECT_EQ(buffer[2], 0xBC);
    EXPECT_EQ(buffer[3], 0x9A);
    EXPECT_EQ(buffer[4], 0x78);
    EXPECT_EQ(buffer[5], 0x56);
    EXPECT_EQ(buffer[6], 0x34);
    EXPECT_EQ(buffer[7], 0x12);
    
    // Verify round-trip
    uint64_t result = ftl::ReadU64LE(buffer);
    EXPECT_EQ(result, original);
}

// Test uint64_t big-endian operations
TEST_F(ByteOrderTest, WriteReadU64BE) {
    uint64_t original = 0x123456789ABCDEF0;
    
    ftl::WriteU64BE(buffer, original);
    
    // Verify byte order in buffer
    EXPECT_EQ(buffer[0], 0x12);
    EXPECT_EQ(buffer[1], 0x34);
    EXPECT_EQ(buffer[2], 0x56);
    EXPECT_EQ(buffer[3], 0x78);
    EXPECT_EQ(buffer[4], 0x9A);
    EXPECT_EQ(buffer[5], 0xBC);
    EXPECT_EQ(buffer[6], 0xDE);
    EXPECT_EQ(buffer[7], 0xF0);
    
    // Verify round-trip
    uint64_t result = ftl::ReadU64BE(buffer);
    EXPECT_EQ(result, original);
}

// Test edge cases
TEST_F(ByteOrderTest, EdgeCases) {
    // Test zero values
    ftl::WriteU16LE(buffer, 0);
    EXPECT_EQ(ftl::ReadU16LE(buffer), 0);
    
    ftl::WriteU32BE(buffer, 0);
    EXPECT_EQ(ftl::ReadU32BE(buffer), 0);
    
    ftl::WriteU64LE(buffer, 0);
    EXPECT_EQ(ftl::ReadU64LE(buffer), 0);
    
    // Test maximum values
    ftl::WriteU16LE(buffer, 0xFFFF);
    EXPECT_EQ(ftl::ReadU16LE(buffer), 0xFFFF);
    
    ftl::WriteU32BE(buffer, 0xFFFFFFFF);
    EXPECT_EQ(ftl::ReadU32BE(buffer), 0xFFFFFFFF);
    
    ftl::WriteU64LE(buffer, 0xFFFFFFFFFFFFFFFF);
    EXPECT_EQ(ftl::ReadU64LE(buffer), 0xFFFFFFFFFFFFFFFF);
}

// Test that LE and BE operations produce different byte patterns for the same value
TEST_F(ByteOrderTest, EndiannessDifference) {
    uint32_t value = 0x12345678;
    uint8_t le_buffer[4];
    uint8_t be_buffer[4];
    
    ftl::WriteU32LE(le_buffer, value);
    ftl::WriteU32BE(be_buffer, value);
    
    // Buffers should contain different byte patterns
    EXPECT_NE(std::memcmp(le_buffer, be_buffer, 4), 0);
    
    // But both should read back to the original value
    EXPECT_EQ(ftl::ReadU32LE(le_buffer), value);
    EXPECT_EQ(ftl::ReadU32BE(be_buffer), value);
}
