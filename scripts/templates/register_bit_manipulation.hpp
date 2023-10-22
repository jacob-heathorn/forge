# pragma once

#include <stdint.h>
#include <bitset>

template<uint32_t N>
void WriteBits(volatile uint32_t &reg, const uint32_t lsb, const std::bitset<N> value) {
	std::bitset<32> reg_bitset(reg);
	for (uint32_t i = 0; i < value.size(); ++i) {
		reg_bitset[i + lsb] = value[i];
	}
	reg = reg_bitset.to_ulong();
}

template<uint32_t N>
std::bitset<N> ReadBits(const volatile uint32_t &reg, const uint32_t lsb) {
	std::bitset<32> reg_bitset(reg);
	std::bitset<N> dest_bitset(reg);
	for (uint32_t i = 0; i < dest_bitset.size(); ++i) {
		dest_bitset[i] = reg_bitset[lsb + i];
	}
	return dest_bitset;
}

inline void WriteBit(volatile uint32_t &reg, uint32_t bit, bool value) {
	WriteBits<1>(reg, bit, value);
}

inline bool ReadBit(const volatile uint32_t &reg, const uint32_t bit) {
	return static_cast<bool>(ReadBits<1>(reg, bit).to_ulong());
}
