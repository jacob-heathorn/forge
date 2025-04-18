#pragma once

#include "ftl/bits/i_mutex.hpp"
#include <cassert>
#include "tx_api.h"

namespace ftl {

static char kMutexName[] = "FtlMutex";

class Mutex : IMutex {
private:
    TX_MUTEX handle_;
    bool initialized_;

public:
    Mutex() noexcept {
        while (TX_SUCCESS != tx_mutex_create(&handle_, kMutexName, TX_INHERIT)) {}
        initialized_ = true;
    }
    ~Mutex() noexcept {
        if (initialized_) {
            tx_mutex_delete(&handle_);
        }
    }
    void lock() {
        if (initialized_) {
            while (TX_SUCCESS != tx_mutex_get(&handle_, TX_WAIT_FOREVER)) {}
        }
    }
    bool try_lock()
    {
        if (!initialized_) return false;
        return (tx_mutex_get(&handle_, TX_NO_WAIT) == TX_SUCCESS);
    }
    void unlock()
    {
        if (initialized_) {
            while (TX_SUCCESS != tx_mutex_put(&handle_)) {}
        }
    }

    Mutex(const Mutex&) = delete;
    Mutex& operator=(const Mutex&) = delete;

    using native_handle_type = void*;
    native_handle_type native_handle() { return &handle_; }
};

} // namespace ftl
