#pragma once

#include <cstdint>
#include <type_traits>

// Type-safe MMIO primitives.
//
// A Register is defined by its address, reset value, access policy, and the list of Fields
// it contains. Operations valid on the register are determined at compile time by the
// access policy and each field's modified-write semantics (SVD <modifiedWriteValues>).
//
// Usage:
//
//   auto snap = nLPI2C5::MSR::read();
//   if (snap.get<nLPI2C5::MsrFields::NDF>()) return Nack;
//
//   nLPI2C5::MSR::clear<nLPI2C5::MsrFields::SDF>();
//
//   nLPI2C5::MTDR::write(nLPI2C5::MtdrFields::CMD{Cmd::Start},
//                        nLPI2C5::MtdrFields::DATA{0x6C});
//
//   nLPI2C5::MCR::modify(nLPI2C5::McrFields::MEN{true});
//
// Misuse is a compile error:
//   - Reading a write-only register
//   - Modifying a read-only register
//   - Passing a W1C/W1S/W1T field value to write()/modify() (use clear/set_flag/toggle)
//   - Reading a write-only field from a Snapshot
//
// Escape hatch: every register exposes ::raw() (volatile uint32_t&), ::kAddr,
// and ::kResetValue so ported C code can be transliterated directly.

namespace ftl::mmio {

// ============================================================================
// Access policies (SVD <access>)
// ============================================================================
struct ReadOnly  {};
struct WriteOnly {};
struct ReadWrite {};

using RO = ReadOnly;
using WO = WriteOnly;
using RW = ReadWrite;

// ============================================================================
// Modified-write semantics (SVD <modifiedWriteValues>)
// ============================================================================
struct Normal        {};  // Standard RW field; user controls every bit.
struct OneToClear    {};  // W1C: write 1 clears, write 0 preserves.
struct OneToSet      {};  // W1S: write 1 sets,   write 0 preserves.
struct OneToToggle   {};  // W1T: write 1 toggles,write 0 preserves.
struct ZeroToClear   {};  // W0C: write 0 clears, write 1 preserves.
struct ZeroToSet     {};  // W0S: write 0 sets,   write 1 preserves.
struct ZeroToToggle  {};  // W0T: write 0 toggles,write 1 preserves.
struct ClearOnWrite  {};  // Any write clears.
struct SetOnWrite    {};  // Any write sets.

// ============================================================================
// Field
//
// Dual role:
//   * As a type (template argument) — carries compile-time metadata:
//       Snapshot::get<Field>(), Register::clear<Field>(), etc.
//   * As a value (constructed instance) — carries a runtime value:
//       Register::write(Field{v}), Register::modify(Field{v}, ...)
// ============================================================================
template <std::uint32_t Width,
          std::uint32_t Offset,
          typename ValueT  = std::uint32_t,
          typename AccessT = ReadWrite,
          typename ModifyT = Normal>
struct Field {
  using value_type  = ValueT;
  using access      = AccessT;
  using modify_kind = ModifyT;

  static constexpr std::uint32_t kWidth  = Width;
  static constexpr std::uint32_t kOffset = Offset;
  static constexpr std::uint32_t kMask   =
      (Width == 32u) ? 0xFFFFFFFFu : (((1u << Width) - 1u) << Offset);

  static_assert(Width >= 1 && Width <= 32, "Field width must be in [1, 32]");
  static_assert(Offset + Width <= 32,      "Field runs past register end");

  static constexpr bool is_readable      = !std::is_same_v<AccessT, WriteOnly>;
  static constexpr bool is_writable      = !std::is_same_v<AccessT, ReadOnly>;
  static constexpr bool is_normal_write  = is_writable && std::is_same_v<ModifyT, Normal>;
  static constexpr bool is_w1c           = std::is_same_v<ModifyT, OneToClear>;
  static constexpr bool is_w1s           = std::is_same_v<ModifyT, OneToSet>;
  static constexpr bool is_w1t           = std::is_same_v<ModifyT, OneToToggle>;
  static constexpr bool is_reserved      = false;

  // Value storage (only meaningful for instances passed to write()/modify()).
  ValueT value;
  constexpr explicit Field(ValueT v) : value(v) {}
};

// Reserved bits: codegen emits one of these for every gap in the SVD field map.
// Preserved on modify() so nothing gets stomped. Never constructed as a value.
template <std::uint32_t Width, std::uint32_t Offset>
struct Reserved {
  static constexpr std::uint32_t kMask =
      (Width == 32u) ? 0xFFFFFFFFu : (((1u << Width) - 1u) << Offset);
  static constexpr bool is_readable      = false;
  static constexpr bool is_writable      = false;
  static constexpr bool is_normal_write  = false;
  static constexpr bool is_w1c           = false;
  static constexpr bool is_w1s           = false;
  static constexpr bool is_w1t           = false;
  static constexpr bool is_reserved      = true;
};

// ============================================================================
// Compile-time mask helpers
// ============================================================================
namespace detail {

template <bool B>
constexpr std::uint32_t mask_if(std::uint32_t m) { return B ? m : 0u; }

template <typename... Fs>
constexpr std::uint32_t reserved_mask()     { std::uint32_t m = 0; ((m |= mask_if<Fs::is_reserved>     (Fs::kMask)), ...); return m; }
template <typename... Fs>
constexpr std::uint32_t normal_write_mask() { std::uint32_t m = 0; ((m |= mask_if<Fs::is_normal_write> (Fs::kMask)), ...); return m; }
template <typename... Fs>
constexpr std::uint32_t w1c_mask()          { std::uint32_t m = 0; ((m |= mask_if<Fs::is_w1c>          (Fs::kMask)), ...); return m; }
template <typename... Fs>
constexpr std::uint32_t w1s_mask()          { std::uint32_t m = 0; ((m |= mask_if<Fs::is_w1s>          (Fs::kMask)), ...); return m; }
template <typename... Fs>
constexpr std::uint32_t w1t_mask()          { std::uint32_t m = 0; ((m |= mask_if<Fs::is_w1t>          (Fs::kMask)), ...); return m; }

}  // namespace detail

// ============================================================================
// Snapshot: value-type view of a register read.
// All .get<F>() calls see the same coherent value; no volatile re-reads.
// ============================================================================
class Snapshot {
 public:
  explicit constexpr Snapshot(std::uint32_t v) : value_(v) {}

  template <typename F>
  constexpr typename F::value_type get() const {
    static_assert(!F::is_reserved, "Cannot read a reserved field");
    static_assert(F::is_readable,  "Field is not readable");
    return static_cast<typename F::value_type>((value_ & F::kMask) >> F::kOffset);
  }

  constexpr std::uint32_t raw() const { return value_; }

 private:
  std::uint32_t value_;
};

// ============================================================================
// Register
// ============================================================================
template <std::uintptr_t Addr,
          std::uint32_t  ResetValue,
          typename       AccessT,
          typename...    Fields>
class Register {
  static constexpr std::uint32_t kReservedMask    = detail::reserved_mask<Fields...>();
  static constexpr std::uint32_t kNormalWriteMask = detail::normal_write_mask<Fields...>();
  static constexpr std::uint32_t kW1cMask         = detail::w1c_mask<Fields...>();
  static constexpr std::uint32_t kW1sMask         = detail::w1s_mask<Fields...>();
  static constexpr std::uint32_t kW1tMask         = detail::w1t_mask<Fields...>();

  // Bits that must be zero on writeback to preserve current hardware state
  // (W1C/W1S/W1T all treat a write of 0 as "no effect").
  static constexpr std::uint32_t kOneNeutralMask  = kW1cMask | kW1sMask | kW1tMask;

  // True iff this register has no RW fields and no reserved bits.
  // In that case, single-flag ops reduce to a one-instruction atomic store.
  static constexpr bool kIsPureStatusFlag =
      (kNormalWriteMask == 0u) && (kReservedMask == 0u);

  // Fold a pack of field-value instances into a (mask, bits) pair.
  template <typename... Args>
  static constexpr std::uint32_t combined_mask() {
    std::uint32_t m = 0;
    ((m |= Args::kMask), ...);
    return m;
  }
  template <typename... Args>
  static std::uint32_t combined_bits(const Args&... args) {
    std::uint32_t v = 0;
    ((v |= ((static_cast<std::uint32_t>(args.value) << Args::kOffset) & Args::kMask)), ...);
    return v;
  }

 public:
  static constexpr std::uintptr_t kAddr       = Addr;
  static constexpr std::uint32_t  kResetValue = ResetValue;

  // Escape hatch — always available regardless of policy.
  static volatile std::uint32_t& raw() {
    return *reinterpret_cast<volatile std::uint32_t*>(Addr);
  }

  // -------------------------- read -----------------------------------------
  template <typename A = AccessT,
            std::enable_if_t<!std::is_same_v<A, WriteOnly>, int> = 0>
  static Snapshot read() {
    return Snapshot{raw()};
  }

  // -------------------------- write (atomic overwrite) ----------------------
  template <typename... Args,
            typename A = AccessT,
            std::enable_if_t<!std::is_same_v<A, ReadOnly>, int> = 0>
  static void write(const Args&... args) {
    static_assert(((!Args::is_reserved) && ...),     "Cannot write a reserved field");
    static_assert((Args::is_writable && ...),         "Field is not writable");
    static_assert((Args::is_normal_write && ...),
        "W1C/W1S/W1T fields cannot be passed to write(); "
        "use Register::clear<F>() / set_flag<F>() / toggle<F>()");
    raw() = combined_bits(args...);
  }

  template <typename A = AccessT,
            std::enable_if_t<!std::is_same_v<A, ReadOnly>, int> = 0>
  static void reset() {
    raw() = ResetValue;
  }

  // -------------------------- modify (RMW, sensitive-bit safe) --------------
  template <typename... Args,
            typename A = AccessT,
            std::enable_if_t<std::is_same_v<A, ReadWrite>, int> = 0>
  static void modify(const Args&... args) {
    static_assert(((!Args::is_reserved) && ...),     "Cannot modify a reserved field");
    static_assert((Args::is_writable && ...),         "Field is not writable");
    static_assert((Args::is_normal_write && ...),
        "W1C/W1S/W1T fields cannot be passed to modify(); "
        "use Register::clear<F>() / set_flag<F>() / toggle<F>()");
    constexpr std::uint32_t mask  = combined_mask<Args...>();
    const     std::uint32_t bits  = combined_bits(args...);
    const     std::uint32_t current = raw();
    // Zero out W1C/W1S/W1T bits so RMW doesn't re-trigger them.
    // Reserved bits are preserved because they're outside `mask`.
    raw() = (current & ~(mask | kOneNeutralMask)) | bits;
  }

  // -------------------------- single-flag ops -------------------------------
  template <typename F>
  static void clear() {
    static_assert(F::is_w1c, "Register::clear<F>() requires a OneToClear field");
    if constexpr (kIsPureStatusFlag) {
      raw() = F::kMask;
    } else {
      raw() = (raw() & ~kOneNeutralMask) | F::kMask;
    }
  }

  template <typename F>
  static void set_flag() {
    static_assert(F::is_w1s, "Register::set_flag<F>() requires a OneToSet field");
    if constexpr (kIsPureStatusFlag) {
      raw() = F::kMask;
    } else {
      raw() = (raw() & ~kOneNeutralMask) | F::kMask;
    }
  }

  template <typename F>
  static void toggle() {
    static_assert(F::is_w1t, "Register::toggle<F>() requires a OneToToggle field");
    if constexpr (kIsPureStatusFlag) {
      raw() = F::kMask;
    } else {
      raw() = (raw() & ~kOneNeutralMask) | F::kMask;
    }
  }
};

}  // namespace ftl::mmio
