#include <iostream>
#include <map>
#include <memory>
#include <string>

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
    }

    template<typename U>
    VerboseAllocator(const VerboseAllocator<U>&) noexcept {
        std::cout << "VerboseAllocator: copy constructor\n";
    }

    T* allocate(size_type n) {
        std::cout << "VerboseAllocator: allocating " << n 
                  << " objects of size " << sizeof(T) 
                  << " (total: " << n * sizeof(T) << " bytes)\n";
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, size_type n) noexcept {
        std::cout << "VerboseAllocator: deallocating " << n 
                  << " objects of size " << sizeof(T) 
                  << " (total: " << n * sizeof(T) << " bytes)\n";
        ::operator delete(p);
    }

    template<typename U, typename... Args>
    void construct(U* p, Args&&... args) {
        std::cout << "VerboseAllocator: constructing object at " << p << "\n";
        new(p) U(std::forward<Args>(args)...);
    }

    template<typename U>
    void destroy(U* p) {
        std::cout << "VerboseAllocator: destroying object at " << p << "\n";
        p->~U();
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