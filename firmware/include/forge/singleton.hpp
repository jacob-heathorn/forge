#pragma once

#include <new>
#include <cassert>
#include <type_traits>
#include <utility>


namespace forge
{

// This template provides a mechanism for ensuring that only one instance of a derived class exists
// throughout the lifetime of the application. The instance is allocated in static storage and can
// be accessed via the instance() method.
template <typename T>
class Singleton {
public:
    // Create the singleton instance in static storage.
    // Must be called exactly once.
    template <typename... Args>
    static void create(Args&&... args) {
        assert(!isCreated() && "Singleton already created!");
        if (!isCreated())
        {
            new (instanceBuffer()) T(std::forward<Args>(args)...);
        }
    }

    // Returns a reference to the singleton instance.
    static T& instance() {
        assert(isCreated() && "Singleton not created! Call create() first.");
        return *reinterpret_cast<T*>(instanceBuffer());
    }

    // Optionally, destroy the singleton (calls its destructor).
    static void destroy() {
        if (isCreated()) {
            instance().~T();
            isCreated() = false;
        }
    }

    // Delete copy and assignment to enforce singleton semantics.
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

protected:
    Singleton()
    {
        assert(!isCreated() && "Singleton already created!");
        isCreated() = true;
    }
    ~Singleton() = default;

private:
    // Use a function that returns a reference to a static storage buffer.
    static void* instanceBuffer() {
        // This static variable is defined when the function is first called.
        // By then, T should be complete.
        static typename std::aligned_storage<sizeof(T), alignof(T)>::type buffer;
        return &buffer;
    }

    // Use a function-local static boolean to track whether the instance was created.
    static bool& isCreated() {
        static bool flag = false;
        return flag;
    }
};

}
