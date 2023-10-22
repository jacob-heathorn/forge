# pragma once

#include "register_bit_manipulation.hpp"

// Encapsulates access to a register
class Register {
public:
  explicit Register(const uint32_t address): register_{reinterpret_cast<uint32_t *>(address)} {};
  
  virtual ~Register() = default;

  Register& operator=(const Register &other) = default;
  
  void Set(uint32_t value) { *register_ = value; }
  uint32_t Get() const { return *register_; }

  template <uint32_t N>
  void SetBits(const uint32_t lsb, const std::bitset<N> value) {
    WriteBits<N>(*register_, lsb, value);
  }

  void SetBit(const uint32_t bit, bool value) {
    WriteBit(*register_, bit, value);
  }
  
  template <uint32_t N>
  std::bitset<N> GetBits(const uint32_t lsb) const {
    return ReadBits<N>(*register_, lsb);
  }

  bool GetBit(const uint32_t bit) const {
    return ReadBit(*register_, bit);
  }

  void operator&=(uint32_t value) {
    *register_ &= value;
  }

  void WaitForBit(const uint32_t bit, bool value) const {
    while (value != this->GetBit(bit)) { /* poll */ }
  }

  template <uint32_t N>
  void WaitForBits(const uint32_t lsb, const std::bitset<N> value) {
    while (value != this->GetBits<N>(lsb)) { /* poll */ }
  }

  uint32_t GetAddress() const {
    return reinterpret_cast<uint32_t>(register_);
  }

private:
  volatile uint32_t *register_;
};
