#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <map>

#include "ftl/allocator/malloc_obj_strategy.hpp"
#include "ftl/map.hpp"

class MapTest : public ::testing::Test {
protected:
    // Common allocator for int->string maps
    using IntStringNode = ftl::Map<int, std::string>::Node;
    ftl::allocator::MallocObjStrategy<IntStringNode> int_string_alloc_;
    
    void SetUp() override {
        // MallocObjStrategy doesn't track allocation count
    }
    
    void TearDown() override {
        // MallocObjStrategy doesn't track allocation count
    }
};

TEST_F(MapTest, DefaultConstruction) {
    ftl::Map<int, std::string> map(int_string_alloc_);
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0u);
}

TEST_F(MapTest, InsertAndFind) {
    ftl::Map<int, std::string> map(int_string_alloc_);
    
    auto result = map.insert({42, "forty-two"});
    EXPECT_TRUE(result.second);
    EXPECT_EQ(result.first->first, 42);
    EXPECT_EQ(result.first->second, "forty-two");
    
    auto it = map.find(42);
    EXPECT_NE(it, map.end());
    EXPECT_EQ(it->first, 42);
    EXPECT_EQ(it->second, "forty-two");
    
    it = map.find(99);
    EXPECT_EQ(it, map.end());
}

TEST_F(MapTest, InsertDuplicate) {
    // Test ftl::Map
    ftl::Map<int, std::string> map(int_string_alloc_);
    
    auto result1 = map.insert({42, "first"});
    EXPECT_TRUE(result1.second);
    
    auto result2 = map.insert({42, "second"});
    EXPECT_FALSE(result2.second);
    EXPECT_EQ(result2.first->second, "first");
    
    EXPECT_EQ(map.size(), 1u);
    
    // Verify std::map has the same behavior
    std::map<int, std::string> std_map;
    
    auto std_result1 = std_map.insert({42, "first"});
    EXPECT_TRUE(std_result1.second);
    
    auto std_result2 = std_map.insert({42, "second"});
    EXPECT_FALSE(std_result2.second);
    EXPECT_EQ(std_result2.first->second, "first");  // std::map also doesn't overwrite
    
    EXPECT_EQ(std_map.size(), 1u);
}

TEST_F(MapTest, OperatorBracket) {
    ftl::Map<int, std::string> map(int_string_alloc_);
    
    map[1] = "one";
    map[2] = "two";
    map[3] = "three";
    
    EXPECT_EQ(map.size(), 3u);
    EXPECT_EQ(map[1], "one");
    EXPECT_EQ(map[2], "two");
    EXPECT_EQ(map[3], "three");
    
    map[2] = "deux";
    EXPECT_EQ(map[2], "deux");
    EXPECT_EQ(map.size(), 3u);
}

TEST_F(MapTest, Erase) {
    ftl::Map<int, std::string> map(int_string_alloc_);
    
    map[1] = "one";
    map[2] = "two";
    map[3] = "three";
    
    EXPECT_EQ(map.size(), 3u);
    
    size_t erased = map.erase(2);
    EXPECT_EQ(erased, 1u);
    EXPECT_EQ(map.size(), 2u);
    EXPECT_EQ(map.find(2), map.end());
    
    erased = map.erase(99);
    EXPECT_EQ(erased, 0u);
    EXPECT_EQ(map.size(), 2u);
}

TEST_F(MapTest, EraseByIterator) {
    ftl::Map<int, std::string> map(int_string_alloc_);
    
    map[1] = "one";
    map[2] = "two";
    map[3] = "three";
    
    auto it = map.find(2);
    EXPECT_NE(it, map.end());
    
    auto next_it = map.erase(it);
    EXPECT_EQ(next_it->first, 3);
    EXPECT_EQ(map.size(), 2u);
    EXPECT_EQ(map.find(2), map.end());
}

TEST_F(MapTest, Clear) {
    ftl::Map<int, std::string> map(int_string_alloc_);
    
    map[1] = "one";
    map[2] = "two";
    map[3] = "three";
    
    EXPECT_FALSE(map.empty());
    
    map.clear();
    
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0u);
    EXPECT_EQ(map.find(1), map.end());
}

TEST_F(MapTest, Iteration) {
    ftl::Map<int, std::string> map(int_string_alloc_);
    
    map[3] = "three";
    map[1] = "one";
    map[4] = "four";
    map[2] = "two";
    
    std::vector<int> keys;
    for (const auto& [key, value] : map) {
        keys.push_back(key);
    }
    
    EXPECT_EQ(keys.size(), 4u);
    EXPECT_EQ(keys[0], 1);
    EXPECT_EQ(keys[1], 2);
    EXPECT_EQ(keys[2], 3);
    EXPECT_EQ(keys[3], 4);
}

TEST_F(MapTest, ReverseIteration) {
    ftl::Map<int, std::string> map(int_string_alloc_);
    
    map[1] = "one";
    map[2] = "two";
    map[3] = "three";
    
    std::vector<int> keys;
    auto it = map.end();
    while (it != map.begin()) {
        --it;
        keys.push_back(it->first);
    }
    
    EXPECT_EQ(keys.size(), 3u);
    EXPECT_EQ(keys[0], 3);
    EXPECT_EQ(keys[1], 2);
    EXPECT_EQ(keys[2], 1);
}

TEST_F(MapTest, Count) {
    ftl::Map<int, std::string> map(int_string_alloc_);
    
    map[1] = "one";
    map[2] = "two";
    
    EXPECT_EQ(map.count(1), 1u);
    EXPECT_EQ(map.count(2), 1u);
    EXPECT_EQ(map.count(3), 0u);
}

TEST_F(MapTest, Emplace) {
    ftl::Map<int, std::string> map(int_string_alloc_);
    
    auto result = map.emplace(42, "forty-two");
    EXPECT_TRUE(result.second);
    EXPECT_EQ(result.first->first, 42);
    EXPECT_EQ(result.first->second, "forty-two");
    
    result = map.emplace(42, "duplicate");
    EXPECT_FALSE(result.second);
    EXPECT_EQ(result.first->second, "forty-two");
}

TEST_F(MapTest, CopyConstruction) {
    ftl::Map<int, std::string> map1(int_string_alloc_);
    map1[1] = "one";
    map1[2] = "two";
    map1[3] = "three";
    
    ftl::Map<int, std::string> map2(map1);
    
    EXPECT_EQ(map2.size(), 3u);
    EXPECT_EQ(map2[1], "one");
    EXPECT_EQ(map2[2], "two");
    EXPECT_EQ(map2[3], "three");
    
    map2[4] = "four";
    EXPECT_EQ(map2.size(), 4u);
    EXPECT_EQ(map1.size(), 3u);
}

TEST_F(MapTest, CopyAssignment) {
    ftl::Map<int, std::string> map1(int_string_alloc_);
    map1[1] = "one";
    map1[2] = "two";
    
    ftl::Map<int, std::string> map2(int_string_alloc_);
    map2[10] = "ten";
    
    map2 = map1;
    
    EXPECT_EQ(map2.size(), 2u);
    EXPECT_EQ(map2[1], "one");
    EXPECT_EQ(map2[2], "two");
    EXPECT_EQ(map2.find(10), map2.end());
}

TEST_F(MapTest, MoveConstruction) {
    ftl::Map<int, std::string> map1(int_string_alloc_);
    map1[1] = "one";
    map1[2] = "two";
    map1[3] = "three";
    
    ftl::Map<int, std::string> map2(std::move(map1));
    
    EXPECT_EQ(map2.size(), 3u);
    EXPECT_EQ(map2[1], "one");
    EXPECT_EQ(map2[2], "two");
    EXPECT_EQ(map2[3], "three");
    
    EXPECT_EQ(map1.size(), 0u);
}

TEST_F(MapTest, LargeDataset) {
    using IntIntNode = ftl::Map<int, int>::Node;
    ftl::allocator::MallocObjStrategy<IntIntNode> int_int_alloc;
    ftl::Map<int, int> map(int_int_alloc);
    
    constexpr int N = 1000;
    for (int i = 0; i < N; ++i) {
        map[i] = i * i;
    }
    
    EXPECT_EQ(map.size(), static_cast<size_t>(N));
    
    for (int i = 0; i < N; ++i) {
        auto it = map.find(i);
        EXPECT_NE(it, map.end());
        EXPECT_EQ(it->second, i * i);
    }
    
    for (int i = 0; i < N; i += 2) {
        map.erase(i);
    }
    
    EXPECT_EQ(map.size(), static_cast<size_t>(N / 2));
    
    for (int i = 0; i < N; ++i) {
        if (i % 2 == 0) {
            EXPECT_EQ(map.find(i), map.end());
        } else {
            EXPECT_NE(map.find(i), map.end());
        }
    }
}

TEST_F(MapTest, RandomInsertDelete) {
    using IntIntNode = ftl::Map<int, int>::Node;
    ftl::allocator::MallocObjStrategy<IntIntNode> int_int_alloc;
    ftl::Map<int, int> map(int_int_alloc);
    std::mt19937 gen(42);
    std::uniform_int_distribution<> dis(1, 100);
    
    for (int i = 0; i < 500; ++i) {
        int key = dis(gen);
        if (dis(gen) % 2 == 0) {
            map[key] = key * 2;
        } else {
            map.erase(key);
        }
    }
    
    size_t count = 0;
    for (auto it = map.begin(); it != map.end(); ++it) {
        EXPECT_EQ(it->second, it->first * 2);
        ++count;
    }
    EXPECT_EQ(count, map.size());
}

TEST_F(MapTest, CustomComparator) {
    struct ReverseCompare {
        bool operator()(int a, int b) const { return a > b; }
    };
    
    using ReverseNode = ftl::Map<int, std::string, ReverseCompare>::Node;
    ftl::allocator::MallocObjStrategy<ReverseNode> reverse_alloc;
    ftl::Map<int, std::string, ReverseCompare> map(reverse_alloc, ReverseCompare{});
    
    map[1] = "one";
    map[2] = "two";
    map[3] = "three";
    
    std::vector<int> keys;
    for (const auto& [key, value] : map) {
        keys.push_back(key);
    }
    
    EXPECT_EQ(keys[0], 3);
    EXPECT_EQ(keys[1], 2);
    EXPECT_EQ(keys[2], 1);
}

TEST_F(MapTest, StringKeys) {
    using StringIntNode = ftl::Map<std::string, int>::Node;
    ftl::allocator::MallocObjStrategy<StringIntNode> string_int_alloc;
    ftl::Map<std::string, int> map(string_int_alloc);
    
    map["apple"] = 1;
    map["banana"] = 2;
    map["cherry"] = 3;
    
    EXPECT_EQ(map["apple"], 1);
    EXPECT_EQ(map["banana"], 2);
    EXPECT_EQ(map["cherry"], 3);
    
    auto it = map.find("banana");
    EXPECT_NE(it, map.end());
    EXPECT_EQ(it->second, 2);
}

TEST_F(MapTest, IteratorPostIncrement) {
    ftl::Map<int, std::string> map(int_string_alloc_);
    map[1] = "one";
    map[2] = "two";
    
    auto it = map.begin();
    auto it_copy = it++;
    
    EXPECT_EQ(it_copy->first, 1);
    EXPECT_EQ(it->first, 2);
}

TEST_F(MapTest, ConstIteration) {
    ftl::Map<int, std::string> map(int_string_alloc_);
    map[1] = "one";
    map[2] = "two";
    
    const auto& const_map = map;
    
    std::vector<int> keys;
    for (auto it = const_map.begin(); it != const_map.end(); ++it) {
        keys.push_back(it->first);
    }
    
    EXPECT_EQ(keys.size(), 2u);
    EXPECT_EQ(keys[0], 1);
    EXPECT_EQ(keys[1], 2);
}

