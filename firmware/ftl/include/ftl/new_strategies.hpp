#pragma once
#include <cstddef>
#include <cstdint>

/*
SafeAlloc: Strategy-Pluggable, No-Virtual Allocation Framework (Header Overview)
Scope
This comment describes the design rationale and invariants for the entire
allocation framework: the strategy “interfaces” (function-pointer based),
ObjectAllocator<T>, and BufferAllocator with fixed size-classes.

Why this design (high level)
• Safety-critical friendly:

Deterministic control flow and bounded time for common paths.

No hidden dynamic dispatch (no virtuals, no RTTI).

No mandatory dynamic memory inside the allocators/strategies.
• Pluggable strategies without inheritance:

Type erasure via small structs containing function pointers + opaque self.

Malloc, fixed pool, bump, DMA-specific pools, etc., can all be wired in.
• Misuse-resistant:

Object allocation path enforces type size at the interface (no size arg).

Buffer path uses fixed size-classes; strategies never receive arbitrary sizes.
• Auditable and testable:

Small surface area, explicit ownership, and simple data flow.

Easy to instrument for high-water marks, exhaustion counts, etc.

Core components
AllocationStrategy (untyped, size baked into state)

Struct of function pointers: void* (*allocate)(void* self) noexcept and
void (*deallocate)(void* self, void* ptr) noexcept, plus opaque self.

No size parameter: the slot’s state holds its block size; prevents
runtime size checks and mismatches by construction.

ObjStrategy<T> (typed, exactly one T)

Same function-pointer pattern but specialized for constructing one T.

Factories like FixedAllocationStrategy::make_for<T>(...) are responsible
for ensuring sizeof(T) matches the strategy’s block size (prefer
static_assert within the factory; return nullptr on configuration errors
in non-constexpr paths).

ObjectAllocator<T>

Accepts only ObjStrategy<T>; performs placement new / explicit dtor.

Returns nullptr on failure (fail-safe result types can be layered later).

No runtime size parameters; alignment is guaranteed by the strategy.

BufferAllocator (fixed size-classes, mapless)

Compile-time (std::array) or runtime (pointer + count) table of:
AllocationStrategy per class and the associated class size.

Chooses the smallest class ≥ requested size (bounded linear or binary search).

Returns a lightweight Buffer handle by value: {ptr, class_size, class_index}.

deallocate(Buffer) uses the embedded class_index—no caller-supplied size.

What we explicitly avoid (and why)
• Virtual functions / inheritance

Removes vtables and RTTI; simplifies WCET analysis and binary review.
• Runtime size checks inside strategies

Avoids interpretive branching per allocation; class size is implicit in state.
• Hidden dynamic memory

Strategies receive externally provided storage (arena base, freelists, etc.).

Allocators themselves never allocate on the heap.
• Global state

All state is passed in (or owned) by concrete strategies; easier to test,
compose, and reason about initialization order.

Determinism & timing
• Fixed-pool strategies: O(1) allocate/free (freelist pop/push).
• Bump strategies: amortized O(1), monotonic, no free (non-safety mode).
• Buffer class selection: bounded O(k) (k = number of classes, typically small).
Switch to constexpr binary search if k grows (sizes sorted ascending).

Alignment & object lifetime
• ObjectAllocator<T> requires strategies that provide storage aligned to
alignof(T) (enforced by factory or documented precondition).
• Object construction uses placement new; destruction uses std::destroy_at.
• Buffer allocations return raw memory blocks; caller owns serialization, etc.

Configuration knobs (typical)
• Compile with -fno-exceptions -fno-rtti.
• Optional macros to enable:

DIAGNOSTICS (exhaustion counters, high-water marks).

POISON_ON_FREE in non-flight builds.

HEADER_CANARIES or pool_id tags to catch cross-free (off by default here).
• Choose std::array flavor when class counts are compile-time constants.

Error handling model (current & future)
• Current: return nullptr on failure; callers handle fail-safe behavior.
• Future (drop-in): wrap pointers in a small result type with error codes
(Exhausted/BadArg/Corrupt), without changing strategy signatures.

Threading & ISR (out of scope here)
• This header does not include synchronization. If used from ISRs or multiple
threads, add either:

Lock-free single-producer/single-consumer freelists, or

Bounded critical sections (interrupt disable windows specified and measured).
• Provide separate strategies for ISR-safe pools if required.

Verification & tests (recommended)
• Unit: allocation/free permutations, pool exhaustion, alignment checks.
• Property: never returns duplicate live pointers; no leaks at steady state.
• Timing: measure allocate/free WCET per strategy.
• Static analysis: MISRA/CERT C++ rules where applicable; forbid dynamic casts,
new/delete in flight code paths, and unbounded recursion.

Portability & code size
• Function-pointer polymorphism avoids template bloat across strategies.
• Only ObjectAllocator<T> is templated; buffer path can be entirely non-templated.
• Works with plain C toolchains at the boundary if needed (extern "C" factories).

Non-goals
• General-purpose malloc replacement (fragmentation not addressed here).
• Automatic resizing/rebalancing across size-classes at runtime.
• Exception-based error signaling.

Typical wiring (examples, not part of this header)
• Fixed object pool:
- Carve N * sizeof(T) bytes + freelist from an arena; build ObjStrategy<T>.
• Fixed buffer classes:
- For classes {64, 128, 256}, create three FixedPool states and three
AllocationStrategys; pass them with the sizes array to BufferAllocator.
• Non-safety mode:
- Use bump or system malloc strategies behind the same interfaces.

Summary
This framework keeps polymorphism, removes virtual machinery, avoids runtime
size arguments in strategies, and keeps all memory usage explicit and bounded.
It is intentionally minimal to make timing, safety, and audit proofs tractable,
while still letting integrators plug in custom, hardware-aware allocators.
*/

// Forward declarations
struct BumpAllocator;
struct Buffer;

// ============================================================================
// Interface allocation strategies, buffers OR objects
// ============================================================================
struct AllocationStrategy {
    // For fixed-size buffers/blocks (size baked into state; no size arg)
    void* (*allocate)(void* self) noexcept;
    void  (*deallocate)(void* self, void* ptr) noexcept;
    void* self{nullptr};
};

// Typed strategy for objects: always allocates exactly one T
template <typename T>
struct ObjStrategy {
    void* (*allocate)(void* self) noexcept;   // returns storage for one T
    void  (*deallocate)(void* self, void* p) noexcept;
    void* self{nullptr};
};


// ===============================
// Public “strategy tags” (factories only; no impl here)
// Each has:
//   - make(block_size, ...) -> AllocationStrategy   [for buffers]
//   - make_for<T>(...)     -> ObjStrategy<T>        [for objects; compile-time size check]
// ===============================
struct MallocAllocationStrategy {
    static AllocationStrategy make(std::size_t block_size) noexcept;
    template <typename T>
    static ObjStrategy<T> make_for() noexcept; // allocates sizeof(T)
};

struct BumpPoolAllocationStrategy {
    static AllocationStrategy make(std::size_t block_size, BumpAllocator& bump) noexcept;
    template <typename T>
    static ObjStrategy<T> make_for(BumpAllocator& bump) noexcept; // grows from bump
};

struct FixedAllocationStrategy {
    static AllocationStrategy make(std::size_t block_size,
                                   std::size_t max_blocks,
                                   BumpAllocator& bump) noexcept;

    template <typename T>
    static ObjStrategy<T> make_for(std::size_t max_blocks, BumpAllocator& bump) noexcept;
    // static_assert inside impl ensures block size == sizeof(T)
};

// ===============================
// Allocators (use any strategy)
// ===============================

// Enforces T at compile time by only accepting ObjStrategy<T>
template <typename T>
class ObjectAllocator {
public:
    explicit ObjectAllocator(ObjStrategy<T> strat) noexcept : strat_(strat) {}

    // placement-new T; returns nullptr on failure
    T* allocate() noexcept {
        if (!strat_.allocate) return nullptr;
        void* raw = strat_.allocate(strat_.self);
        if (!raw) return nullptr;
        return new (raw) T();
    }

    void deallocate(T* p) noexcept {
        if (!p) return;
        p->~T();
        if (strat_.deallocate) strat_.deallocate(strat_.self, p);
    }

private:
    ObjStrategy<T> strat_{};
};

// ============================================================================
// Buffer allocator (untyped) with size-class map
// ============================================================================
// Mapless allocator with compile-time size classes.
template <std::size_t... CLASSES>
class BufferAllocator {
public:
  static constexpr std::size_t kNum = sizeof...(CLASSES);
  static constexpr std::array<std::size_t, kNum> kSizes{CLASSES...};

  using StrategyArray = std::array<AllocationStrategy, kNum>;

  explicit BufferAllocator(const StrategyArray& strategies) noexcept
    : strategies_(strategies) {}

  // Smallest class >= requested size. Returns {nullptr,0,0} on failure.
  Buffer allocate(std::size_t req_size) noexcept {
    const std::size_t idx = choose_class(req_size);
    if (idx == npos) return {};
    auto& s = strategies_[idx];
    if (!s.allocate) return {};
    void* p = s.allocate(s.self);
    if (!p) return {};
    return Buffer{p, kSizes[idx], static_cast<std::uint16_t>(idx)};
  }

  // Dealloc uses class_index embedded in the Buffer. Safe, no req_size needed.
  void deallocate(Buffer b) noexcept {
    if (!b.ptr) return;
    const std::size_t ci = b.class_index;
    if (ci >= kNum) return;                // sanity guard
    auto& s = strategies_[ci];
    if (s.deallocate) s.deallocate(s.self, b.ptr);
  }

private:
  static constexpr std::size_t npos = static_cast<std::size_t>(-1);

  // O(k) linear scan (k is tiny). If k grows, switch to constexpr binary search.
  static constexpr std::size_t choose_class(std::size_t n) noexcept {
    for (std::size_t i = 0; i < kNum; ++i) if (n <= kSizes[i]) return i;
    return npos;
  }

  StrategyArray strategies_{};
};
