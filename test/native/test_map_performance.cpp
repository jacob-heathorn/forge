#include <iostream>
#include <map>
#include <chrono>
#include <vector>
#include <algorithm>
#include <random>
#include <iomanip>

#include "ftl/bump_allocator.hpp"
#include "ftl/allocator/bump_pool_strategy.hpp"
#include "ftl/map.hpp"

static constexpr size_t POOL_MEMORY_SIZE = 10 * 1024 * 1024; // 10MB for performance tests

int main() {
    using namespace std::chrono;
    
    // Initialize memory pool for ftl::Map
    uint8_t* buffer = new uint8_t[POOL_MEMORY_SIZE];
    ftl::BumpAllocator allocator(buffer, POOL_MEMORY_SIZE);
    
    // Create allocators for different node types
    using IntMapNode = ftl::Map<int, int>::Node;
    ftl::allocator::BumpPoolObjStrategy<IntMapNode> int_pool_alloc(allocator);  // Use bump pool strategy
    
    // Test parameters
    const std::vector<size_t> test_sizes = {100, 1000, 10000, 50000};
    const size_t num_lookups = 10000;  // Number of random lookups to perform
    
    std::cout << "\n=== Map Performance Comparison ===\n";
    std::cout << "Comparing ftl::Map vs std::map\n";
    std::cout << "Pool memory size: " << POOL_MEMORY_SIZE / (1024*1024) << " MB\n\n";
    
    for (size_t N : test_sizes) {
        std::cout << "Dataset size: " << N << " elements\n";
        std::cout << "========================================\n";
        
        // Generate test data
        std::vector<std::pair<int, int>> test_data;
        test_data.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            test_data.push_back({static_cast<int>(i), static_cast<int>(i * i)});
        }
        
        // Shuffle for random insertion order
        std::mt19937 gen(42);
        std::shuffle(test_data.begin(), test_data.end(), gen);
        
        // Create random lookup keys
        std::vector<int> lookup_keys;
        lookup_keys.reserve(num_lookups);
        std::uniform_int_distribution<> dis(0, static_cast<int>(N - 1));
        for (size_t i = 0; i < num_lookups; ++i) {
            lookup_keys.push_back(dis(gen));
        }
        
        double ftl_insert_time, ftl_lookup_time, ftl_iter_time, ftl_erase_time;
        double std_insert_time, std_lookup_time, std_iter_time, std_erase_time;
        
        // Test ftl::Map
        {
            ftl::Map<int, int> ftl_map(int_pool_alloc);
            
            // Warm up
            for (int i = 0; i < 10; ++i) {
                ftl_map[i] = i;
            }
            ftl_map.clear();
            
            // Measure insertion time
            auto start = high_resolution_clock::now();
            for (const auto& [key, value] : test_data) {
                ftl_map.insert({key, value});
            }
            auto end = high_resolution_clock::now();
            ftl_insert_time = duration_cast<nanoseconds>(end - start).count() / 1000.0;
            
            // Measure lookup time
            volatile int dummy = 0; // Prevent optimization
            start = high_resolution_clock::now();
            for (int key : lookup_keys) {
                auto it = ftl_map.find(key);
                if (it != ftl_map.end()) {
                    dummy += it->second;
                }
            }
            end = high_resolution_clock::now();
            ftl_lookup_time = duration_cast<nanoseconds>(end - start).count() / 1000.0;
            
            // Measure iteration time
            start = high_resolution_clock::now();
            for (const auto& [key, value] : ftl_map) {
                dummy += value;
            }
            end = high_resolution_clock::now();
            ftl_iter_time = duration_cast<nanoseconds>(end - start).count() / 1000.0;
            
            // Measure erase time (erase half the elements)
            start = high_resolution_clock::now();
            for (size_t i = 0; i < N; i += 2) {
                ftl_map.erase(static_cast<int>(i));
            }
            end = high_resolution_clock::now();
            ftl_erase_time = duration_cast<nanoseconds>(end - start).count() / 1000.0;
        }
        
        // Test std::map
        {
            std::map<int, int> std_map;
            
            // Warm up
            for (int i = 0; i < 10; ++i) {
                std_map[i] = i;
            }
            std_map.clear();
            
            // Measure insertion time
            auto start = high_resolution_clock::now();
            for (const auto& [key, value] : test_data) {
                std_map.insert({key, value});
            }
            auto end = high_resolution_clock::now();
            std_insert_time = duration_cast<nanoseconds>(end - start).count() / 1000.0;
            
            // Measure lookup time
            volatile int dummy = 0; // Prevent optimization
            start = high_resolution_clock::now();
            for (int key : lookup_keys) {
                auto it = std_map.find(key);
                if (it != std_map.end()) {
                    dummy += it->second;
                }
            }
            end = high_resolution_clock::now();
            std_lookup_time = duration_cast<nanoseconds>(end - start).count() / 1000.0;
            
            // Measure iteration time
            start = high_resolution_clock::now();
            for (const auto& [key, value] : std_map) {
                dummy += value;
            }
            end = high_resolution_clock::now();
            std_iter_time = duration_cast<nanoseconds>(end - start).count() / 1000.0;
            
            // Measure erase time (erase half the elements)
            start = high_resolution_clock::now();
            for (size_t i = 0; i < N; i += 2) {
                std_map.erase(static_cast<int>(i));
            }
            end = high_resolution_clock::now();
            std_erase_time = duration_cast<nanoseconds>(end - start).count() / 1000.0;
        }
        
        // Print results with comparison
        std::cout << std::fixed << std::setprecision(2);
        
        std::cout << "\nOperation Times (microseconds):\n";
        std::cout << "----------------------------------------\n";
        std::cout << "                ftl::Map    std::map    Speedup\n";
        std::cout << "Insert " << std::setw(6) << N << ":  " 
                  << std::setw(9) << ftl_insert_time << "  " 
                  << std::setw(9) << std_insert_time << "   "
                  << std::setw(5) << std_insert_time / ftl_insert_time << "x\n";
        
        std::cout << "Lookup " << std::setw(6) << num_lookups << ": " 
                  << std::setw(9) << ftl_lookup_time << "  " 
                  << std::setw(9) << std_lookup_time << "   "
                  << std::setw(5) << std_lookup_time / ftl_lookup_time << "x\n";
        
        std::cout << "Iterate all:    " 
                  << std::setw(9) << ftl_iter_time << "  " 
                  << std::setw(9) << std_iter_time << "   "
                  << std::setw(5) << std_iter_time / ftl_iter_time << "x\n";
        
        std::cout << "Erase " << std::setw(6) << N/2 << ":  " 
                  << std::setw(9) << ftl_erase_time << "  " 
                  << std::setw(9) << std_erase_time << "   "
                  << std::setw(5) << std_erase_time / ftl_erase_time << "x\n";
        
        std::cout << "\nPer-Element Times (nanoseconds):\n";
        std::cout << "----------------------------------------\n";
        std::cout << "                ftl::Map    std::map\n";
        std::cout << "Insert:     " 
                  << std::setw(12) << (ftl_insert_time * 1000) / N << "  " 
                  << std::setw(10) << (std_insert_time * 1000) / N << "\n";
        std::cout << "Lookup:     " 
                  << std::setw(12) << (ftl_lookup_time * 1000) / num_lookups << "  " 
                  << std::setw(10) << (std_lookup_time * 1000) / num_lookups << "\n";
        std::cout << "Erase:      " 
                  << std::setw(12) << (ftl_erase_time * 1000) / (N/2) << "  " 
                  << std::setw(10) << (std_erase_time * 1000) / (N/2) << "\n";
        
        std::cout << "\n";
    }
    
    // Memory usage comparison
    std::cout << "\n=== Memory Usage Analysis ===\n";
    std::cout << "========================================\n";
    
    const int mem_test_size = 10000;
    
    // Test ftl::Map memory usage
    {
        ftl::Map<int, int> ftl_map(int_pool_alloc);
        
        for (int i = 0; i < mem_test_size; ++i) {
            ftl_map[i] = i * i;
        }
        
        using MapNode = ftl::Map<int, int>::Node;
        
        std::cout << "ftl::Map (" << mem_test_size << " elements):\n";
        std::cout << "  Elements in map: " << ftl_map.size() << "\n";
        std::cout << "  Memory per node: " << sizeof(MapNode) << " bytes\n";
        std::cout << "  Total memory used: ~" << ftl_map.size() * sizeof(MapNode) << " bytes\n";
        
        // Clean up
        ftl_map.clear();
        
        std::cout << "\nAfter clear():\n";
        std::cout << "  Elements in map: " << ftl_map.size() << "\n";
    }
    
    std::cout << "\nstd::map (" << mem_test_size << " elements):\n";
    std::cout << "  Estimated memory per node: ~" << sizeof(std::_Rb_tree_node<std::pair<const int, int>>) << " bytes\n";
    std::cout << "  Estimated total memory: ~" << mem_test_size * sizeof(std::_Rb_tree_node<std::pair<const int, int>>) << " bytes\n";
    std::cout << "  (Note: std::map uses dynamic allocation with additional overhead)\n";
    
    // Performance summary
    std::cout << "\n=== Summary ===\n";
    std::cout << "========================================\n";
    std::cout << "ftl::Map advantages:\n";
    std::cout << "  - Predictable memory allocation from pool\n";
    std::cout << "  - Better cache locality\n";
    std::cout << "  - No heap fragmentation\n";
    std::cout << "  - Faster allocation/deallocation\n";
    std::cout << "\nstd::map advantages:\n";
    std::cout << "  - No fixed memory pool size limit\n";
    std::cout << "  - Memory returned to system on clear()\n";
    std::cout << "  - Move semantics fully supported\n";
    
    delete[] buffer;
    
    return 0;
}