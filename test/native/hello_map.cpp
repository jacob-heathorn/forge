#include <iostream>
#include <map>
#include <string>
#include "ftl/bump_pool_allocator.hpp"

int main() {
    std::cout << "=== Creating map with custom allocator ===\n\n";
    
    // Create a bump allocator with a static buffer
    static uint8_t memory[64 * 1024]; // 64KB buffer
    ftl::BumpAllocator allocator(memory, sizeof(memory));
    
    // Map uses std::_Rb_tree_node internally, so we need to initialize the pool for that type
    using NodeType = std::_Rb_tree_node<std::pair<const int, std::string>>;
    ftl::BumpPoolAllocator<NodeType>::initializePool(allocator);
    
    using MapType = std::map<int, std::string, std::less<int>, 
                             ftl::BumpPoolAllocator<std::pair<const int, std::string>>>;
    
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