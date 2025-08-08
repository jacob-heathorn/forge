#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <map>

#include "ftl/bump_allocator.hpp"
#include "ftl/bump_pool_allocator.hpp"
#include "ftl/map.hpp"

static constexpr size_t POOL_MEMORY_SIZE = 1024 * 1024;

// Global test environment that persists across all tests
class MapTestEnvironment : public ::testing::Environment {
public:
    // Define node types for different map types
    using IntStringNode = ftl::Map<int, std::string>::Node;
    using IntIntNode = ftl::Map<int, int>::Node;
    using StringIntNode = ftl::Map<std::string, int>::Node;
    
    // Custom comparator type for reverse comparison test
    struct ReverseCompare {
        bool operator()(int a, int b) const { return a > b; }
    };
    using IntStringReverseNode = ftl::Map<int, std::string, ReverseCompare>::Node;
    
    static uint8_t* buffer_;
    static ftl::BumpAllocator* alloc_;
    static ftl::BumpPoolAllocator<IntStringNode>* int_string_pool_;
    static ftl::BumpPoolAllocator<IntIntNode>* int_int_pool_;
    static ftl::BumpPoolAllocator<StringIntNode>* string_int_pool_;
    static ftl::BumpPoolAllocator<IntStringReverseNode>* int_string_reverse_pool_;

    void SetUp() override {
        buffer_ = new uint8_t[POOL_MEMORY_SIZE];
        alloc_ = new ftl::BumpAllocator(buffer_, POOL_MEMORY_SIZE);
        int_string_pool_ = new ftl::BumpPoolAllocator<IntStringNode>(*alloc_, 100);
        int_int_pool_ = new ftl::BumpPoolAllocator<IntIntNode>(*alloc_, 100);
        string_int_pool_ = new ftl::BumpPoolAllocator<StringIntNode>(*alloc_, 100);
        int_string_reverse_pool_ = new ftl::BumpPoolAllocator<IntStringReverseNode>(*alloc_, 100);
    }

    void TearDown() override {
        delete int_string_reverse_pool_;
        delete string_int_pool_;
        delete int_int_pool_;
        delete int_string_pool_;
        delete alloc_;
        delete[] buffer_;
    }
};

uint8_t* MapTestEnvironment::buffer_ = nullptr;
ftl::BumpAllocator* MapTestEnvironment::alloc_ = nullptr;
ftl::BumpPoolAllocator<MapTestEnvironment::IntStringNode>* MapTestEnvironment::int_string_pool_ = nullptr;
ftl::BumpPoolAllocator<MapTestEnvironment::IntIntNode>* MapTestEnvironment::int_int_pool_ = nullptr;
ftl::BumpPoolAllocator<MapTestEnvironment::StringIntNode>* MapTestEnvironment::string_int_pool_ = nullptr;
ftl::BumpPoolAllocator<MapTestEnvironment::IntStringReverseNode>* MapTestEnvironment::int_string_reverse_pool_ = nullptr;

class MapTest : public ::testing::Test {
protected:
    using IntStringNode = ftl::Map<int, std::string>::Node;
    using IntIntNode = ftl::Map<int, int>::Node;
    using StringIntNode = ftl::Map<std::string, int>::Node;
    using IntStringReverseNode = MapTestEnvironment::IntStringReverseNode;
    
    ftl::BumpPoolAllocator<IntStringNode>* int_string_pool_;
    ftl::BumpPoolAllocator<IntIntNode>* int_int_pool_;
    ftl::BumpPoolAllocator<StringIntNode>* string_int_pool_;
    ftl::BumpPoolAllocator<IntStringReverseNode>* int_string_reverse_pool_;

    void SetUp() override {
        int_string_pool_ = MapTestEnvironment::int_string_pool_;
        int_int_pool_ = MapTestEnvironment::int_int_pool_;
        string_int_pool_ = MapTestEnvironment::string_int_pool_;
        int_string_reverse_pool_ = MapTestEnvironment::int_string_reverse_pool_;
        
        // Verify clean state before test for all pools
        VerifyPoolClean(int_string_pool_, "int-string");
        VerifyPoolClean(int_int_pool_, "int-int");
        VerifyPoolClean(string_int_pool_, "string-int");
        VerifyPoolClean(int_string_reverse_pool_, "int-string-reverse");
    }
    
    void TearDown() override {
        // Verify all memory was properly deallocated after test
        VerifyPoolClean(int_string_pool_, "int-string", true);
        VerifyPoolClean(int_int_pool_, "int-int", true);
        VerifyPoolClean(string_int_pool_, "string-int", true);
        VerifyPoolClean(int_string_reverse_pool_, "int-string-reverse", true);
    }
    
private:
    template<typename T>
    void VerifyPoolClean(ftl::BumpPoolAllocator<T>* pool, const char* name, bool after_test = false) {
        auto used = pool->UsedSize();
        auto free = pool->FreeSize();
        auto total = pool->TotalSize();
        
        if (after_test) {
            ASSERT_EQ(used, 0u) << "Memory leak in " << name << " pool: " << used << " nodes still in use after test";
            if (total > 0) {
                ASSERT_EQ(free, total) << "Not all nodes returned to " << name << " pool after test";
            }
        } else {
            if (total > 0) {
                ASSERT_EQ(used, 0u) << "Memory leak in " << name << " pool: " << used << " nodes still in use before test";
                ASSERT_EQ(free, total) << "Not all nodes are free in " << name << " pool before test";
            }
        }
    }
};

TEST_F(MapTest, DefaultConstruction) {
    ftl::Map<int, std::string> map(*int_string_pool_);
    EXPECT_TRUE(map.empty());
    EXPECT_EQ(map.size(), 0u);
}

TEST_F(MapTest, InsertAndFind) {
    ftl::Map<int, std::string> map(*int_string_pool_);
    
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
    ftl::Map<int, std::string> map(*int_string_pool_);
    
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
    ftl::Map<int, std::string> map(*int_string_pool_);
    
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
    ftl::Map<int, std::string> map(*int_string_pool_);
    
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
    ftl::Map<int, std::string> map(*int_string_pool_);
    
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
    ftl::Map<int, std::string> map(*int_string_pool_);
    
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
    ftl::Map<int, std::string> map(*int_string_pool_);
    
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
    ftl::Map<int, std::string> map(*int_string_pool_);
    
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
    ftl::Map<int, std::string> map(*int_string_pool_);
    
    map[1] = "one";
    map[2] = "two";
    
    EXPECT_EQ(map.count(1), 1u);
    EXPECT_EQ(map.count(2), 1u);
    EXPECT_EQ(map.count(3), 0u);
}

TEST_F(MapTest, Emplace) {
    ftl::Map<int, std::string> map(*int_string_pool_);
    
    auto result = map.emplace(42, "forty-two");
    EXPECT_TRUE(result.second);
    EXPECT_EQ(result.first->first, 42);
    EXPECT_EQ(result.first->second, "forty-two");
    
    result = map.emplace(42, "duplicate");
    EXPECT_FALSE(result.second);
    EXPECT_EQ(result.first->second, "forty-two");
}

TEST_F(MapTest, CopyConstruction) {
    ftl::Map<int, std::string> map1(*int_string_pool_);
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
    ftl::Map<int, std::string> map1(*int_string_pool_);
    map1[1] = "one";
    map1[2] = "two";
    
    ftl::Map<int, std::string> map2(*int_string_pool_);
    map2[10] = "ten";
    
    map2 = map1;
    
    EXPECT_EQ(map2.size(), 2u);
    EXPECT_EQ(map2[1], "one");
    EXPECT_EQ(map2[2], "two");
    EXPECT_EQ(map2.find(10), map2.end());
}

TEST_F(MapTest, MoveConstruction) {
    ftl::Map<int, std::string> map1(*int_string_pool_);
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
    ftl::Map<int, int> map(*int_int_pool_);
    
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
    ftl::Map<int, int> map(*int_int_pool_);
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
    using ReverseCompare = MapTestEnvironment::ReverseCompare;
    
    // Custom comparator map needs its own node type
    ftl::Map<int, std::string, ReverseCompare> map(*int_string_reverse_pool_, ReverseCompare{});
    
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
    ftl::Map<std::string, int> map(*string_int_pool_);
    
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
    ftl::Map<int, std::string> map(*int_string_pool_);
    map[1] = "one";
    map[2] = "two";
    
    auto it = map.begin();
    auto it_copy = it++;
    
    EXPECT_EQ(it_copy->first, 1);
    EXPECT_EQ(it->first, 2);
}

TEST_F(MapTest, ConstIteration) {
    ftl::Map<int, std::string> map(*int_string_pool_);
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

// Register the test environment
// This will be called automatically by gtest
namespace {
    ::testing::Environment* const map_env = 
        ::testing::AddGlobalTestEnvironment(new MapTestEnvironment);
}