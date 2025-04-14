#include "tx_api.h"
#include <cassert>

namespace ftl {

// TODO. 
//<Func, Arg>
// TxThreadFunction

class TxThread {
 public:
  // The thread entry in our implementation is a simple function pointer with signature void(void).
  TxThread(const char* name,
           void (*func)(),
           void* stack,
           ULONG stack_size,
           UINT priority,
           UINT preempt_thresh,
           ULONG time_slice,
           UINT auto_start)
      : user_func_(func) {
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

  ~TxThread() {
    UINT status = tx_thread_terminate(&handle_);
    assert(status == TX_SUCCESS && "tx_thread_terminate failed");
    status = tx_thread_delete(&handle_);
    assert(status == TX_SUCCESS && "tx_thread_delete failed");
  }

  TxThread(const TxThread&) = delete;
  TxThread& operator=(const TxThread&) = delete;
  TxThread(TxThread&&) = delete;
  TxThread& operator=(TxThread&&) = delete;

 private:
  TX_THREAD handle_;
  void (*user_func_)();

  static void thread_entry_helper(ULONG input) {
    auto* self = reinterpret_cast<TxThread*>(input);
    self->user_func_();
  }
};

}
