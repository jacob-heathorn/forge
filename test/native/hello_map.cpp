#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <cassert>
#include "ftl/bump_pool.hpp"
#include "ftl/bump_allocator.hpp"

template<typename T>
class VerboseAllocator {
public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::true_type;

    VerboseAllocator() noexcept {
        std::cout << "VerboseAllocator: default constructor\n";
        initializePoolIfNeeded();
    }

    template<typename U>
    VerboseAllocator(const VerboseAllocator<U>&) noexcept {
        std::cout << "VerboseAllocator: copy constructor\n";
        initializePoolIfNeeded();
    }

    T* allocate(size_type n) {
        assert(n == 1 && "VerboseAllocator only supports allocating one object at a time");
        std::cout << "VerboseAllocator: allocating " << n 
                  << " objects of size " << sizeof(T) 
                  << " (total: " << n * sizeof(T) << " bytes)\n";
        
        T* ptr = getPool().acquire();
        std::cout << "  -> allocated from bump_pool at " << ptr << "\n";
        return ptr;
    }

    void deallocate(T* p, size_type n) noexcept {
        assert(n == 1 && "VerboseAllocator only supports deallocating one object at a time");
        std::cout << "VerboseAllocator: deallocating " << n 
                  << " objects of size " << sizeof(T) 
                  << " (total: " << n * sizeof(T) << " bytes) at " << p << "\n";
        
        getPool().release(p);
        std::cout << "  -> returned to bump_pool\n";
    }

private:
    static ftl::BumpPool<T>& getPool() {
        static uint8_t memory[64 * 1024]; // 64KB static buffer
        static ftl::BumpAllocator allocator(memory, sizeof(memory));
        static ftl::BumpPool<T> pool(allocator, 10); // Pre-allocate 10 nodes
        return pool;
    }

    static void initializePoolIfNeeded() {
        // Pool is initialized with static initialization above
        static bool initialized = false;
        if (!initialized) {
            std::cout << "  -> Static bump_pool initialized for type\n";
            initialized = true;
        }
    }
};

template<typename T, typename U>
bool operator==(const VerboseAllocator<T>&, const VerboseAllocator<U>&) {
    return true;
}

template<typename T, typename U>
bool operator!=(const VerboseAllocator<T>&, const VerboseAllocator<U>&) {
    return false;
}

int main() {
    std::cout << "=== Creating map with custom allocator ===\n\n";
    
    using MapType = std::map<int, std::string, std::less<int>, 
                             VerboseAllocator<std::pair<const int, std::string>>>;
    
    MapType myMap;
    
    std::cout << "\n=== Inserting elements ===\n\n";
    
    myMap[1] = "one";
    myMap[2] = "two";
    myMap[3] = "three";
    
    std::cout << "\n=== Iterating over map ===\n\n";
    
    for (const auto& [key, value] : myMap) {
        std::cout << "Key: " << key << ", Value: " << value << "\n";
    }
    
    std::cout << "\n=== Erasing element with key=2 ===\n\n";
    
    myMap.erase(2);
    
    std::cout << "\n=== Final map contents ===\n\n";
    
    for (const auto& [key, value] : myMap) {
        std::cout << "Key: " << key << ", Value: " << value << "\n";
    }
    
    std::cout << "\n=== Map going out of scope (cleanup) ===\n\n";
    
    return 0;
}