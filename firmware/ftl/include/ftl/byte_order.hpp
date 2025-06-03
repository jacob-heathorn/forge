#pragma once

#include <cstdint>

namespace ftl {

// Inline functions for efficient byte serialization with explicit endianness

// Write uint16_t in little-endian format
inline void WriteU16LE(uint8_t* dest, uint16_t value) {
    dest[0] = static_cast<uint8_t>(value & 0xFF);
    dest[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

// Write uint16_t in big-endian format
inline void WriteU16BE(uint8_t* dest, uint16_t value) {
    dest[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
    dest[1] = static_cast<uint8_t>(value & 0xFF);
}

// Write uint32_t in little-endian format
inline void WriteU32LE(uint8_t* dest, uint32_t value) {
    dest[0] = static_cast<uint8_t>(value & 0xFF);
    dest[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    dest[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    dest[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

// Write uint32_t in big-endian format
inline void WriteU32BE(uint8_t* dest, uint32_t value) {
    dest[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
    dest[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
    dest[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
    dest[3] = static_cast<uint8_t>(value & 0xFF);
}

// Read uint16_t from little-endian format
inline uint16_t ReadU16LE(const uint8_t* src) {
    return static_cast<uint16_t>(src[0])
         | (static_cast<uint16_t>(src[1]) << 8);
}

// Read uint16_t from big-endian format
inline uint16_t ReadU16BE(const uint8_t* src) {
    return (static_cast<uint16_t>(src[0]) << 8)
         | static_cast<uint16_t>(src[1]);
}

// Read uint32_t from little-endian format
inline uint32_t ReadU32LE(const uint8_t* src) {
    return static_cast<uint32_t>(src[0])
         | (static_cast<uint32_t>(src[1]) << 8)
         | (static_cast<uint32_t>(src[2]) << 16)
         | (static_cast<uint32_t>(src[3]) << 24);
}

// Read uint32_t from big-endian format
inline uint32_t ReadU32BE(const uint8_t* src) {
    return (static_cast<uint32_t>(src[0]) << 24)
         | (static_cast<uint32_t>(src[1]) << 16)
         | (static_cast<uint32_t>(src[2]) << 8)
         | static_cast<uint32_t>(src[3]);
}

// Write uint64_t in little-endian format
inline void WriteU64LE(uint8_t* dest, uint64_t value) {
    dest[0] = static_cast<uint8_t>(value & 0xFF);
    dest[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    dest[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    dest[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
    dest[4] = static_cast<uint8_t>((value >> 32) & 0xFF);
    dest[5] = static_cast<uint8_t>((value >> 40) & 0xFF);
    dest[6] = static_cast<uint8_t>((value >> 48) & 0xFF);
    dest[7] = static_cast<uint8_t>((value >> 56) & 0xFF);
}

// Write uint64_t in big-endian format
inline void WriteU64BE(uint8_t* dest, uint64_t value) {
    dest[0] = static_cast<uint8_t>((value >> 56) & 0xFF);
    dest[1] = static_cast<uint8_t>((value >> 48) & 0xFF);
    dest[2] = static_cast<uint8_t>((value >> 40) & 0xFF);
    dest[3] = static_cast<uint8_t>((value >> 32) & 0xFF);
    dest[4] = static_cast<uint8_t>((value >> 24) & 0xFF);
    dest[5] = static_cast<uint8_t>((value >> 16) & 0xFF);
    dest[6] = static_cast<uint8_t>((value >> 8) & 0xFF);
    dest[7] = static_cast<uint8_t>(value & 0xFF);
}

// Read uint64_t from little-endian format
inline uint64_t ReadU64LE(const uint8_t* src) {
    return static_cast<uint64_t>(src[0])
         | (static_cast<uint64_t>(src[1]) << 8)
         | (static_cast<uint64_t>(src[2]) << 16)
         | (static_cast<uint64_t>(src[3]) << 24)
         | (static_cast<uint64_t>(src[4]) << 32)
         | (static_cast<uint64_t>(src[5]) << 40)
         | (static_cast<uint64_t>(src[6]) << 48)
         | (static_cast<uint64_t>(src[7]) << 56);
}

// Read uint64_t from big-endian format
inline uint64_t ReadU64BE(const uint8_t* src) {
    return (static_cast<uint64_t>(src[0]) << 56)
         | (static_cast<uint64_t>(src[1]) << 48)
         | (static_cast<uint64_t>(src[2]) << 40)
         | (static_cast<uint64_t>(src[3]) << 32)
         | (static_cast<uint64_t>(src[4]) << 24)
         | (static_cast<uint64_t>(src[5]) << 16)
         | (static_cast<uint64_t>(src[6]) << 8)
         | static_cast<uint64_t>(src[7]);
}

}
