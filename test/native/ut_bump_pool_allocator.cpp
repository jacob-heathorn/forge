#include <gtest/gtest.h>
#include <map>
#include <list>
#include <set>
#include "ftl/bump_pool_allocator.hpp"
#include "ftl/bump_allocator.hpp"

namespace {

// Test fixture for BumpPoolAllocator tests
class BumpPoolAllocatorTest : public ::testing::Test {
protected:
    static constexpr size_t BUFFER_SIZE = 16 * 1024;
    uint8_t buffer_[BUFFER_SIZE];
    ftl::BumpAllocator allocator_{buffer_, BUFFER_SIZE};

    void SetUp() override {
        // Reset allocator for each test
        allocator_.reset();
    }
};

// Simple test struct for containers
struct TestNode {
    int key;
    int value;
    
    TestNode(int k = 0, int v = 0) : key(k), value(v) {}
    bool operator<(const TestNode& other) const { return key < other.key; }
};

TEST_F(BumpPoolAllocatorTest, BasicMapOperations) {
    // Initialize pool for map nodes
    using MapNodeType = std::pair<const int, std::string>;
    ftl::BumpPoolAllocator<MapNodeType>::initializePool(allocator_, 10);
    
    // Create map with custom allocator
    std::map<int, std::string, std::less<int>, ftl::BumpPoolAllocator<MapNodeType>> myMap;
    
    // Test insertions
    myMap[1] = "one";
    myMap[2] = "two";
    myMap[3] = "three";
    
    EXPECT_EQ(myMap.size(), 3);
    EXPECT_EQ(myMap[1], "one");
    EXPECT_EQ(myMap[2], "two");
    EXPECT_EQ(myMap[3], "three");
    
    // Test iteration
    int count = 0;
    for (const auto& pair : myMap) {
        ++count;
        EXPECT_TRUE(pair.first >= 1 && pair.first <= 3);
    }
    EXPECT_EQ(count, 3);
    
    // Test erasure
    myMap.erase(2);
    EXPECT_EQ(myMap.size(), 2);
    EXPECT_EQ(myMap.find(2), myMap.end());
}

TEST_F(BumpPoolAllocatorTest, ListOperations) {
    // Initialize pool for list nodes
    ftl::BumpPoolAllocator<int>::initializePool(allocator_, 20);
    
    // Create list with custom allocator
    std::list<int, ftl::BumpPoolAllocator<int>> myList;
    
    // Test push operations
    for (int i = 0; i < 10; ++i) {
        myList.push_back(i);
    }
    
    EXPECT_EQ(myList.size(), 10);
    
    // Test front and back
    EXPECT_EQ(myList.front(), 0);
    EXPECT_EQ(myList.back(), 9);
    
    // Test iteration and values
    int expected = 0;
    for (int val : myList) {
        EXPECT_EQ(val, expected++);
    }
    
    // Test removal
    myList.remove(5);
    EXPECT_EQ(myList.size(), 9);
}

TEST_F(BumpPoolAllocatorTest, SetOperations) {
    // Initialize pool for set nodes
    ftl::BumpPoolAllocator<TestNode>::initializePool(allocator_, 15);
    
    // Create set with custom allocator
    std::set<TestNode, std::less<TestNode>, ftl::BumpPoolAllocator<TestNode>> mySet;
    
    // Test insertions
    mySet.insert(TestNode(3, 30));
    mySet.insert(TestNode(1, 10));
    mySet.insert(TestNode(4, 40));
    mySet.insert(TestNode(1, 100)); // Duplicate key, should not insert
    mySet.insert(TestNode(5, 50));
    mySet.insert(TestNode(9, 90));
    
    EXPECT_EQ(mySet.size(), 5); // Only 5 unique keys
    
    // Test ordering (set should maintain sorted order)
    auto it = mySet.begin();
    EXPECT_EQ(it->key, 1);
    ++it;
    EXPECT_EQ(it->key, 3);
    ++it;
    EXPECT_EQ(it->key, 4);
    ++it;
    EXPECT_EQ(it->key, 5);
    ++it;
    EXPECT_EQ(it->key, 9);
    
    // Test find
    auto found = mySet.find(TestNode(4, 0));
    EXPECT_NE(found, mySet.end());
    EXPECT_EQ(found->value, 40);
}

TEST_F(BumpPoolAllocatorTest, MultipleContainerTypes) {
    // Initialize pools for different types
    ftl::BumpPoolAllocator<int>::initializePool(allocator_, 10);
    
    using MapNodeType = std::pair<const int, int>;
    ftl::BumpPoolAllocator<MapNodeType>::initializePool(allocator_, 10);
    
    // Create multiple containers
    std::list<int, ftl::BumpPoolAllocator<int>> list1;
    std::list<int, ftl::BumpPoolAllocator<int>> list2;
    std::map<int, int, std::less<int>, ftl::BumpPoolAllocator<MapNodeType>> map1;
    
    // Use them
    list1.push_back(1);
    list1.push_back(2);
    list2.push_back(3);
    list2.push_back(4);
    map1[10] = 100;
    map1[20] = 200;
    
    EXPECT_EQ(list1.size(), 2);
    EXPECT_EQ(list2.size(), 2);
    EXPECT_EQ(map1.size(), 2);
}

TEST_F(BumpPoolAllocatorTest, AllocatorComparison) {
    // All allocators of the same type should compare equal
    ftl::BumpPoolAllocator<int> alloc1;
    ftl::BumpPoolAllocator<int> alloc2;
    
    EXPECT_TRUE(alloc1 == alloc2);
    EXPECT_FALSE(alloc1 != alloc2);
}

TEST_F(BumpPoolAllocatorTest, RebindAllocator) {
    // Test rebinding from one type to another
    ftl::BumpPoolAllocator<int>::initializePool(allocator_, 5);
    
    using MapType = std::map<int, double, std::less<int>, 
                            ftl::BumpPoolAllocator<std::pair<const int, double>>>;
    
    // The map internally rebinds the allocator for its node type
    MapType myMap;
    
    // This should work even though we initialized for int, not pair<const int, double>
    // because the map will initialize its own pool when needed
    ftl::BumpPoolAllocator<std::pair<const int, double>>::initializePool(allocator_, 5);
    
    myMap[1] = 1.5;
    myMap[2] = 2.5;
    
    EXPECT_EQ(myMap.size(), 2);
    EXPECT_DOUBLE_EQ(myMap[1], 1.5);
}

TEST_F(BumpPoolAllocatorTest, PoolExhaustion) {
    // Initialize a very small pool
    ftl::BumpPoolAllocator<int>::initializePool(allocator_, 2);
    
    std::list<int, ftl::BumpPoolAllocator<int>> myList;
    
    // These should succeed
    myList.push_back(1);
    myList.push_back(2);
    
    // Remove one to free space
    myList.pop_front();
    
    // This should succeed (reusing freed node)
    myList.push_back(3);
    
    EXPECT_EQ(myList.size(), 2);
}

}  // namespace