#include <iostream>
#include <string>
#include "ftl/map.hpp"
#include "ftl/bump_pool_allocator.hpp"

int main() {
    std::cout << "=== Creating map with custom allocator ===\n\n";
    
    // Create a bump allocator with a static buffer
    static uint8_t memory[64 * 1024]; // 64KB buffer
    ftl::BumpAllocator allocator(memory, sizeof(memory));
    
    // Define the map type
    using ValueType = std::pair<const int, std::string>;
    using MapType = ftl::Map<int, std::string, std::less<int>, 
                             ftl::BumpPoolAllocator<ValueType>>;
    
    // Initialize pool for both the Node type and the value type
    using NodeType = MapType::Node;
    ftl::BumpPoolAllocator<NodeType>::initializePool(allocator);
    ftl::BumpPoolAllocator<ValueType>::initializePool(allocator);
    
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