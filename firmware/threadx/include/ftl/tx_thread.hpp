#ifndef FTL_TX_THREAD_HPP
#define FTL_TX_THREAD_HPP

#include "tx_api.h"
#include <cassert>
#include "etl/delegate.h"  // Include ETL delegate header
#include "etl/optional.h"

namespace ftl {

class TxThread {
 public:
  // Constructor accepts an ETL delegate callback (of type void(void))
  TxThread(const char* name,
           etl::delegate<void(void)> callback,
           void* stack,
           ULONG stack_size,
           UINT priority,
           etl::optional<UINT> preempt_thresh = etl::nullopt,
           ULONG time_slice = TX_NO_TIME_SLICE,
           UINT auto_start = TX_AUTO_START)
      : callback_(callback)
  {
    UINT status = tx_thread_create(
        &handle_,
        const_cast<char*>(name),
        thread_entry_helper,
        reinterpret_cast<ULONG>(this),
        stack,
        stack_size,
        priority,
        preempt_thresh.value_or(priority),
        time_slice,
        auto_start);
    assert(status == TX_SUCCESS && "tx_thread_create failed");
    (void)status; // Prevent unused variable warning in release builds.
  }

  ~TxThread() {
    UINT status = tx_thread_terminate(&handle_);
    assert(status == TX_SUCCESS && "tx_thread_terminate failed");
    status = tx_thread_delete(&handle_);
    assert(status == TX_SUCCESS && "tx_thread_delete failed");
    (void)status; // Prevent unused variable warning in release builds.
  }

  // Disable copying and moving.
  TxThread(const TxThread&) = delete;
  TxThread& operator=(const TxThread&) = delete;
  TxThread(TxThread&&) = delete;
  TxThread& operator=(TxThread&&) = delete;

 private:
  TX_THREAD handle_;
  etl::delegate<void(void)> callback_;

  // Thread entry point: call the stored delegate.
  static void thread_entry_helper(ULONG input) {
    TxThread* self = reinterpret_cast<TxThread*>(input);
    self->callback_();
  }
};

} // namespace ftl

#endif  // FTL_TX_THREAD_HPP
