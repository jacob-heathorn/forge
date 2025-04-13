#include "ftl/functional.hpp"  // Our function wrapper
#include "tx_api.h"
#include <cassert>

namespace ftl {

class thread {
 public:
  // The thread entry in our implementation is a callable with signature void(void*).
  thread(const char* name,
         ftl::function<void(void*)> func,
         void* arg,
         void* stack,
         ULONG stack_size,
         UINT priority,
         UINT preempt_thresh,
         ULONG time_slice,
         UINT auto_start)
      : user_callable_(func), user_arg_(arg) {
    UINT status = tx_thread_create(
        &handle_,
        const_cast<char*>(name),
        thread_entry_helper,
        reinterpret_cast<ULONG>(this),
        stack,
        stack_size,
        priority,
        preempt_thresh,
        time_slice,
        auto_start);

    assert(status == TX_SUCCESS && "tx_thread_create failed");
  }

  ~thread() {
    UINT status = tx_thread_terminate(&handle_);
    assert(status == TX_SUCCESS && "tx_thread_terminate failed");
    status = tx_thread_delete(&handle_);
    assert(status == TX_SUCCESS && "tx_thread_delete failed");
  }

  thread(const thread&) = delete;
  thread& operator=(const thread&) = delete;
  thread(thread&&) = delete;
  thread& operator=(thread&&) = delete;

 private:
  TX_THREAD handle_;
  ftl::function<void(void*)> user_callable_;
  void* user_arg_;

  static void thread_entry_helper(ULONG input) {
    auto* self = reinterpret_cast<thread*>(input);
    self->user_callable_(self->user_arg_);
  }
};

}  // namespace ftl
