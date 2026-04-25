#pragma once

#include <concepts>
#include <cstdint>
#include <type_traits>

// Type-safe MMIO primitives.
//
// A Register pairs a constexpr address with a list of Field types describing
// its layout. Operations valid on a register are gated at compile time by its
// access policy and each field's modified-write semantics (SVD
// <modifiedWriteValues>).
//
//   regs::lpi2c5::MSR::clear<MSR::SDF>();           // W1C
//   regs::lpi2c5::MTDR::write(MTDR::CMD{Cmd::Start},
//                             MTDR::DATA{0x6C});
//   regs::lpi2c5::MCR::modify(MCR::MEN{true});
//   if (regs::lpi2c5::MSR::read().get<MSR::NDF>()) return Nack;
//
// Misuse is a compile error: reading a write-only register, modifying a
// read-only register, passing a W1C/W1S/W1T field to write()/modify() (use
// clear/set/toggle), reading a write-only field from a Snapshot.
//
// Escape hatch: ::raw() (volatile StorageT&), ::kAddr, ::kResetValue.

namespace ftl::mmio {

// -- Access policies (SVD <access>) ------------------------------------------
struct ReadOnly  {};
struct WriteOnly {};
struct ReadWrite {};

using RO = ReadOnly;
using WO = WriteOnly;
using RW = ReadWrite;

// -- Modified-write semantics (SVD <modifiedWriteValues>) --------------------
struct Normal        {};  // Standard RW: caller controls every bit.
struct OneToClear    {};  // W1C: write 1 clears, write 0 preserves.
struct OneToSet      {};  // W1S: write 1 sets,   write 0 preserves.
struct OneToToggle   {};  // W1T: write 1 toggles,write 0 preserves.
struct ZeroToClear   {};  // W0C: write 0 clears, write 1 preserves.
struct ZeroToSet     {};  // W0S: write 0 sets,   write 1 preserves.
struct ZeroToToggle  {};  // W0T: write 0 toggles,write 1 preserves.
struct ClearOnWrite  {};  // Any write clears.
struct SetOnWrite    {};  // Any write sets.

// -- Field --------------------------------------------------------------------
// Dual role: as a type — Snapshot::get<F>(), Register::clear<F>() ...
//            as a value — Register::write(F{v}), Register::modify(F{v}, ...).
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

  ValueT value;
  constexpr explicit Field(ValueT v) : value(v) {}
};

// Codegen emits one Reserved per gap so modify() can preserve those bits.
// Never constructed as a value.
template <std::uint32_t Width, std::uint32_t Offset>
struct Reserved {
  static constexpr std::uint32_t kMask =
      (Width == 32u) ? 0xFFFFFFFFu : (((1u << Width) - 1u) << Offset);
  static constexpr bool is_readable     = false;
  static constexpr bool is_writable     = false;
  static constexpr bool is_normal_write = false;
  static constexpr bool is_w1c          = false;
  static constexpr bool is_w1s          = false;
  static constexpr bool is_w1t          = false;
  static constexpr bool is_reserved     = true;
};

// -- Concepts -----------------------------------------------------------------
template <typename F> concept ReadableField = !F::is_reserved && F::is_readable;
template <typename F> concept NormalWrite   = F::is_normal_write;
template <typename F> concept W1c           = F::is_w1c;
template <typename F> concept W1s           = F::is_w1s;
template <typename F> concept W1t           = F::is_w1t;

template <typename A> concept ReadAccess  = !std::same_as<A, WriteOnly>;
template <typename A> concept WriteAccess = !std::same_as<A, ReadOnly>;
template <typename A> concept RWAccess    = std::same_as<A, ReadWrite>;

// -- Compile-time mask helpers ------------------------------------------------
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

// -- Snapshot -----------------------------------------------------------------
// Value-type view of a register read; .get<F>() calls all see one coherent
// value (no second volatile load).
class Snapshot {
 public:
  explicit constexpr Snapshot(std::uint32_t v) : value_(v) {}

  template <ReadableField F>
  constexpr typename F::value_type get() const {
    return static_cast<typename F::value_type>((value_ & F::kMask) >> F::kOffset);
  }

  constexpr std::uint32_t raw() const { return value_; }

 private:
  std::uint32_t value_;
};

// -- Register -----------------------------------------------------------------
// StorageT (uint8/16/32) is the MMIO access width — set by SVD <size>. Field
// math runs as uint32_t; StorageT only dictates the load/store width at the
// final volatile access, so 8-bit registers compile to strb and unaligned
// 32-bit stores never happen.
template <std::uintptr_t Addr,
          typename       StorageT,
          StorageT       ResetValue,
          typename       AccessT,
          typename...    Fields>
class Register {
  static constexpr std::uint32_t kReservedMask    = detail::reserved_mask<Fields...>();
  static constexpr std::uint32_t kNormalWriteMask = detail::normal_write_mask<Fields...>();
  static constexpr std::uint32_t kW1cMask         = detail::w1c_mask<Fields...>();
  static constexpr std::uint32_t kW1sMask         = detail::w1s_mask<Fields...>();
  static constexpr std::uint32_t kW1tMask         = detail::w1t_mask<Fields...>();

  // Bits to zero on writeback so RMW doesn't re-trigger sticky semantics
  // (writing 0 to a W1C/W1S/W1T bit is a no-op on hardware).
  static constexpr std::uint32_t kOneNeutralMask = kW1cMask | kW1sMask | kW1tMask;

  // True iff register has no normal-RW or reserved bits — flag ops can store
  // the mask directly without RMW.
  static constexpr bool kIsPureStatusFlag =
      (kNormalWriteMask == 0u) && (kReservedMask == 0u);

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
  using storage_type = StorageT;
  static constexpr std::uintptr_t kAddr       = Addr;
  static constexpr StorageT       kResetValue = ResetValue;

  // Always available regardless of access policy.
  static volatile StorageT& raw() {
    return *reinterpret_cast<volatile StorageT*>(Addr);
  }

  static Snapshot read() requires ReadAccess<AccessT> {
    return Snapshot{static_cast<std::uint32_t>(raw())};
  }

  template <NormalWrite... Args>
  static void write(const Args&... args) requires WriteAccess<AccessT> {
    raw() = static_cast<StorageT>(combined_bits(args...));
  }

  static void reset() requires WriteAccess<AccessT> {
    raw() = ResetValue;
  }

  template <NormalWrite... Args>
  static void modify(const Args&... args) requires RWAccess<AccessT> {
    constexpr std::uint32_t mask    = combined_mask<Args...>();
    const     std::uint32_t bits    = combined_bits(args...);
    const     std::uint32_t current = static_cast<std::uint32_t>(raw());
    raw() = static_cast<StorageT>((current & ~(mask | kOneNeutralMask)) | bits);
  }

  // -- Sticky-flag ops --------------------------------------------------------
  // Single store with an OR of the field masks; bits not in the mask write 0,
  // a no-op on hardware for the matching W1-style semantics.
  template <W1c... Fs>
  static void clear() {
    static_assert(sizeof...(Fs) > 0, "clear<>() needs at least one field");
    constexpr std::uint32_t mask = (Fs::kMask | ... | 0u);
    if constexpr (kIsPureStatusFlag) {
      raw() = static_cast<StorageT>(mask);
    } else {
      raw() = static_cast<StorageT>((static_cast<std::uint32_t>(raw()) & ~kOneNeutralMask) | mask);
    }
  }

  template <W1s... Fs>
  static void set() {
    static_assert(sizeof...(Fs) > 0, "set<>() needs at least one field");
    constexpr std::uint32_t mask = (Fs::kMask | ... | 0u);
    if constexpr (kIsPureStatusFlag) {
      raw() = static_cast<StorageT>(mask);
    } else {
      raw() = static_cast<StorageT>((static_cast<std::uint32_t>(raw()) & ~kOneNeutralMask) | mask);
    }
  }

  template <W1t... Fs>
  static void toggle() {
    static_assert(sizeof...(Fs) > 0, "toggle<>() needs at least one field");
    constexpr std::uint32_t mask = (Fs::kMask | ... | 0u);
    if constexpr (kIsPureStatusFlag) {
      raw() = static_cast<StorageT>(mask);
    } else {
      raw() = static_cast<StorageT>((static_cast<std::uint32_t>(raw()) & ~kOneNeutralMask) | mask);
    }
  }
};

}  // namespace ftl::mmio
