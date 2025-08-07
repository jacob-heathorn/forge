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

    // Static method to initialize the pool - must be called before using the allocator
    static void initializePool(ftl::BumpAllocator& allocator) {
        assert(!pool_ && "Pool already initialized");
        std::cout << "VerboseAllocator: Initializing pool.\n";
        pool_ = new ftl::BumpPool<T>(allocator);
    }

    VerboseAllocator() noexcept {
        std::cout << "VerboseAllocator: default constructor\n";
        assert(pool_ && "Pool not initialized! Call VerboseAllocator::initializePool() first");
    }

    template<typename U>
    VerboseAllocator(const VerboseAllocator<U>&) noexcept {
        std::cout << "VerboseAllocator: copy constructor\n";
        assert(pool_ && "Pool not initialized! Call VerboseAllocator::initializePool() first");
    }

    T* allocate(size_type n) {
        assert(n == 1 && "VerboseAllocator only supports allocating one object at a time");
        assert(pool_ && "Pool not initialized! Call VerboseAllocator::initializePool() first");
        
        std::cout << "VerboseAllocator: allocating " << n 
                  << " objects of size " << sizeof(T) 
                  << " (total: " << n * sizeof(T) << " bytes)\n";
        
        T* ptr = pool_->acquire();
        std::cout << "  -> allocated from bump_pool at " << ptr << "\n";
        return ptr;
    }

    void deallocate(T* p, size_type n) noexcept {
        assert(n == 1 && "VerboseAllocator only supports deallocating one object at a time");
        assert(pool_ && "Pool not initialized! Call VerboseAllocator::initializePool() first");
        
        std::cout << "VerboseAllocator: deallocating " << n 
                  << " objects of size " << sizeof(T) 
                  << " (total: " << n * sizeof(T) << " bytes) at " << p << "\n";
        
        pool_->release(p);
        std::cout << "  -> returned to bump_pool\n";
    }

private:
    inline static ftl::BumpPool<T>* pool_ = nullptr;
};

int main() {
    std::cout << "=== Creating map with custom allocator ===\n\n";
    
    // Create a bump allocator with a static buffer
    static uint8_t memory[64 * 1024]; // 64KB buffer
    ftl::BumpAllocator allocator(memory, sizeof(memory));
    
    // Map uses std::_Rb_tree_node internally, so we need to initialize the pool for that type
    using NodeType = std::_Rb_tree_node<std::pair<const int, std::string>>;
    VerboseAllocator<NodeType>::initializePool(allocator);
    
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