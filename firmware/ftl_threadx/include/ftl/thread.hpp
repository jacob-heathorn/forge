#pragma once

#include <cassert>
#include "tx_api.h"

namespace ftl {

class thread {
 public:
  // Use a function pointer type directly
  using entry_fn = void (*)(void*);

  thread(const char* name,
         entry_fn fn,
         void* user_data,
         void* stack,
         ULONG stack_size,
         UINT priority,
         UINT preempt_thresh,
         ULONG time_slice,
         UINT auto_start)
      : name_{name}, fn_{fn}, user_data_{user_data} {
    // Verify stack alignment (8-byte for Cortex-M7)
    assert(((uintptr_t)stack % 8) == 0 && "Stack must be 8-byte aligned");

    UINT status = tx_thread_create(
        &handle_,
        const_cast<char*>(name_),     // CHAR* expected
        thread_entry,
        reinterpret_cast<ULONG>(this),  // Pass 'this' as thread argument
        stack,
        stack_size,
        priority,
        preempt_thresh,
        time_slice,
        auto_start);

    if (status != TX_SUCCESS) {
      assert(false && "Thread creation failed");
    }
  }

  ~thread() {
    tx_thread_terminate(&handle_);
    tx_thread_delete(&handle_);
  }

  // Delete copy and move
  thread(const thread&) = delete;
  thread& operator=(const thread&) = delete;
  thread(thread&&) = delete;
  thread& operator=(thread&&) = delete;

 private:
  TX_THREAD handle_;
  const char* name_;
  entry_fn fn_;         // Function pointer
  void* user_data_;

  static void thread_entry(ULONG input) {
    auto* self = reinterpret_cast<thread*>(input);
    if (self->fn_) {
      self->fn_(self->user_data_);
    }
  }
};

}  // namespace ftl