#pragma once

#include <utility>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include "ftl/allocator/obj_allocator.hpp"
#include "ftl/allocator/strategy.hpp"


/// @brief Red-Black Tree based associative container (Map)
/// 
/// This is a self-balancing binary search tree implementation that maintains
/// O(log n) time complexity for insertions, deletions, and lookups.
/// 
/// Memory Management:
/// - Uses BumpPoolAllocator for memory allocation
/// - All nodes are explicitly deallocated in destructor and clear()
/// - Special 'nil' sentinel node is used instead of nullptr for leaves
/// 
/// Red-Black Tree Properties:
/// 1. Every node is either red or black
/// 2. Root is always black
/// 3. All leaves (nil) are black
/// 4. Red nodes cannot have red children
/// 5. All paths from any node to its descendant nil nodes have the same number of black nodes

namespace ftl {

template<typename Key, typename T, typename Compare = std::less<Key>>
class Map {
public:
    using key_type = Key;
    using mapped_type = T;
    using value_type = std::pair<const Key, T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using key_compare = Compare;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

private:
    /// @brief Node colors for Red-Black tree balancing
    enum class Color : bool { RED = false, BLACK = true };

public:
    /// @brief Internal node structure for the Red-Black tree
    struct Node {
        value_type data;
        Node* parent;
        Node* left;
        Node* right;
    private:
        Color color;
        friend class Map;
    public:
        template<typename... Args>
        Node(Args&&... args) 
            : data(std::forward<Args>(args)...)
            , parent(nullptr)
            , left(nullptr)
            , right(nullptr)
            , color(Color::RED) {}
    };

private:
    Node* root_;                    ///< Root of the tree (nil_ when empty)
    Node* nil_;                     ///< Sentinel node representing all leaves (never deallocated until destructor)
    size_type size_;                ///< Number of elements in the map
    Compare comp_;                  ///< Comparison function object
    allocator::ObjAllocator<Node>& alloc_;      ///< Object allocator reference for Node objects

    /// @brief Initialize the sentinel nil node
    /// The nil node is allocated once and persists for the lifetime of the map
    /// All leaf pointers point to this single nil node to save memory
    void initialize_nil() {
        nil_ = alloc_.allocate();
        if (!nil_) {
            // Cannot recover from nil allocation failure
            std::abort();
        }
        nil_->parent = nil_->left = nil_->right = nil_;
        nil_->color = Color::BLACK;  // nil is always black by RB-tree definition
    }

    /// @brief Update nil's pointers for efficient iteration
    /// nil->parent points to root for tree traversal
    /// nil->left points to minimum element (for begin())
    /// nil->right points to maximum element (for rbegin()/operator--)
    void update_nil_pointers() {
        // Update nil's parent to point to root
        nil_->parent = root_;
        
        // Update nil's left to point to minimum
        if (root_ != nil_) {
            Node* min = root_;
            while (min->left != nil_) {
                min = min->left;
            }
            nil_->left = min;
            
            // Update nil's right to point to maximum
            Node* max = root_;
            while (max->right != nil_) {
                max = max->right;
            }
            nil_->right = max;
        } else {
            // Empty tree
            nil_->left = nil_;
            nil_->right = nil_;
        }
    }

    /// @brief Perform left rotation around node x
    /// Used to maintain Red-Black tree properties during insertions/deletions
    /// @param x Node to rotate around
    void left_rotate(Node* x) {
        Node* y = x->right;
        x->right = y->left;
        if (y->left != nil_) {
            y->left->parent = x;
        }
        y->parent = x->parent;
        if (x->parent == nil_) {
            root_ = y;
        } else if (x == x->parent->left) {
            x->parent->left = y;
        } else {
            x->parent->right = y;
        }
        y->left = x;
        x->parent = y;
    }

    /// @brief Perform right rotation around node x
    /// Mirror operation of left_rotate
    /// @param x Node to rotate around
    void right_rotate(Node* x) {
        Node* y = x->left;
        x->left = y->right;
        if (y->right != nil_) {
            y->right->parent = x;
        }
        y->parent = x->parent;
        if (x->parent == nil_) {
            root_ = y;
        } else if (x == x->parent->right) {
            x->parent->right = y;
        } else {
            x->parent->left = y;
        }
        y->right = x;
        x->parent = y;
    }

    /// @brief Fix Red-Black tree violations after insertion
    /// @param z Newly inserted node (initially red)
    /// Fixes violations by recoloring and rotating nodes
    void insert_fixup(Node* z) {
        while (z->parent->color == Color::RED) {
            if (z->parent == z->parent->parent->left) {
                Node* y = z->parent->parent->right;
                if (y->color == Color::RED) {
                    z->parent->color = Color::BLACK;
                    y->color = Color::BLACK;
                    z->parent->parent->color = Color::RED;
                    z = z->parent->parent;
                } else {
                    if (z == z->parent->right) {
                        z = z->parent;
                        left_rotate(z);
                    }
                    z->parent->color = Color::BLACK;
                    z->parent->parent->color = Color::RED;
                    right_rotate(z->parent->parent);
                }
            } else {
                Node* y = z->parent->parent->left;
                if (y->color == Color::RED) {
                    z->parent->color = Color::BLACK;
                    y->color = Color::BLACK;
                    z->parent->parent->color = Color::RED;
                    z = z->parent->parent;
                } else {
                    if (z == z->parent->left) {
                        z = z->parent;
                        right_rotate(z);
                    }
                    z->parent->color = Color::BLACK;
                    z->parent->parent->color = Color::RED;
                    left_rotate(z->parent->parent);
                }
            }
        }
        root_->color = Color::BLACK;
    }

    /// @brief Replace subtree rooted at u with subtree rooted at v
    /// @param u Node to be replaced
    /// @param v Node to replace with
    /// Used during deletion operations
    void transplant(Node* u, Node* v) {
        if (u->parent == nil_) {
            root_ = v;
        } else if (u == u->parent->left) {
            u->parent->left = v;
        } else {
            u->parent->right = v;
        }
        v->parent = u->parent;
    }

    /// @brief Find the minimum node in subtree rooted at x
    /// @param x Root of subtree
    /// @return Leftmost node in the subtree
    Node* minimum(Node* x) const {
        while (x->left != nil_) {
            x = x->left;
        }
        return x;
    }

    /// @brief Fix Red-Black tree violations after deletion
    /// @param x Node that may violate RB properties
    /// Restores RB properties through recoloring and rotations
    void delete_fixup(Node* x) {
        while (x != root_ && x->color == Color::BLACK) {
            if (x == x->parent->left) {
                Node* w = x->parent->right;
                if (w->color == Color::RED) {
                    w->color = Color::BLACK;
                    x->parent->color = Color::RED;
                    left_rotate(x->parent);
                    w = x->parent->right;
                }
                if (w->left->color == Color::BLACK && w->right->color == Color::BLACK) {
                    w->color = Color::RED;
                    x = x->parent;
                } else {
                    if (w->right->color == Color::BLACK) {
                        w->left->color = Color::BLACK;
                        w->color = Color::RED;
                        right_rotate(w);
                        w = x->parent->right;
                    }
                    w->color = x->parent->color;
                    x->parent->color = Color::BLACK;
                    w->right->color = Color::BLACK;
                    left_rotate(x->parent);
                    x = root_;
                }
            } else {
                Node* w = x->parent->left;
                if (w->color == Color::RED) {
                    w->color = Color::BLACK;
                    x->parent->color = Color::RED;
                    right_rotate(x->parent);
                    w = x->parent->left;
                }
                if (w->right->color == Color::BLACK && w->left->color == Color::BLACK) {
                    w->color = Color::RED;
                    x = x->parent;
                } else {
                    if (w->left->color == Color::BLACK) {
                        w->right->color = Color::BLACK;
                        w->color = Color::RED;
                        left_rotate(w);
                        w = x->parent->left;
                    }
                    w->color = x->parent->color;
                    x->parent->color = Color::BLACK;
                    w->left->color = Color::BLACK;
                    right_rotate(x->parent);
                    x = root_;
                }
            }
        }
        x->color = Color::BLACK;
    }

    /// @brief Recursively destroy all nodes in the tree
    /// @param node Root of subtree to destroy
    /// IMPORTANT: This deallocates all nodes except nil_
    /// Post-order traversal ensures children are freed before parents
    void destroy_tree(Node* node) {
        if (node && node != nil_) {
            destroy_tree(node->left);    // Recursively destroy left subtree
            destroy_tree(node->right);   // Recursively destroy right subtree
            alloc_.deallocate(node); // Return memory to pool (destructor called by pool)
        }
    }

public:
    class iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = Map::value_type;
        using difference_type = Map::difference_type;
        using pointer = Map::pointer;
        using reference = Map::reference;

    private:
        Node* node_;
        Node* nil_;

        Node* tree_minimum(Node* x) const {
            while (x->left != nil_) {
                x = x->left;
            }
            return x;
        }

        Node* tree_maximum(Node* x) const {
            while (x->right != nil_) {
                x = x->right;
            }
            return x;
        }

    public:
        iterator(Node* node, Node* nil) : node_(node), nil_(nil) {}

        reference operator*() const { return node_->data; }
        pointer operator->() const { return &node_->data; }

        iterator& operator++() {
            if (node_->right != nil_) {
                node_ = tree_minimum(node_->right);
            } else {
                Node* y = node_->parent;
                while (y != nil_ && node_ == y->right) {
                    node_ = y;
                    y = y->parent;
                }
                node_ = y;
            }
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        iterator& operator--() {
            // Special case: if we're at end (nil), go to the rightmost element
            if (node_ == nil_) {
                // nil_->right should point to the maximum element
                node_ = nil_->right;
                // If tree is empty, stay at nil
                if (node_ == nil_) {
                    return *this;
                }
            } else if (node_->left != nil_) {
                node_ = tree_maximum(node_->left);
            } else {
                Node* y = node_->parent;
                while (y != nil_ && node_ == y->left) {
                    node_ = y;
                    y = y->parent;
                }
                node_ = y;
            }
            return *this;
        }

        iterator operator--(int) {
            iterator tmp = *this;
            --(*this);
            return tmp;
        }

        bool operator==(const iterator& other) const { return node_ == other.node_; }
        bool operator!=(const iterator& other) const { return node_ != other.node_; }

        friend class Map;
    };

    class const_iterator {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = Map::value_type;
        using difference_type = Map::difference_type;
        using pointer = Map::const_pointer;
        using reference = Map::const_reference;

    private:
        const Node* node_;
        const Node* nil_;

        const Node* tree_minimum(const Node* x) const {
            while (x->left != nil_) {
                x = x->left;
            }
            return x;
        }

        const Node* tree_maximum(const Node* x) const {
            while (x->right != nil_) {
                x = x->right;
            }
            return x;
        }

    public:
        const_iterator(const Node* node, const Node* nil) : node_(node), nil_(nil) {}
        const_iterator(const iterator& it) : node_(it.node_), nil_(it.nil_) {}

        reference operator*() const { return node_->data; }
        pointer operator->() const { return &node_->data; }

        const_iterator& operator++() {
            if (node_->right != nil_) {
                node_ = tree_minimum(node_->right);
            } else {
                const Node* y = node_->parent;
                while (y != nil_ && node_ == y->right) {
                    node_ = y;
                    y = y->parent;
                }
                node_ = y;
            }
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        const_iterator& operator--() {
            // Special case: if we're at end (nil), go to the rightmost element
            if (node_ == nil_) {
                // nil_->right should point to the maximum element
                node_ = nil_->right;
                // If tree is empty, stay at nil
                if (node_ == nil_) {
                    return *this;
                }
            } else if (node_->left != nil_) {
                node_ = tree_maximum(node_->left);
            } else {
                const Node* y = node_->parent;
                while (y != nil_ && node_ == y->left) {
                    node_ = y;
                    y = y->parent;
                }
                node_ = y;
            }
            return *this;
        }

        const_iterator operator--(int) {
            const_iterator tmp = *this;
            --(*this);
            return tmp;
        }

        bool operator==(const const_iterator& other) const { return node_ == other.node_; }
        bool operator!=(const const_iterator& other) const { return node_ != other.node_; }
    };

    /// @brief Construct an empty map
    /// @param allocator Reference to object allocator for Node objects (must outlive the map)
    /// @param comp Comparison function object
    explicit Map(allocator::ObjAllocator<Node>& allocator, const Compare& comp = Compare())
        : root_(nullptr), size_(0), comp_(comp), alloc_(allocator) {
        initialize_nil();  // Allocate sentinel node
        root_ = nil_;      // Empty tree points to nil
    }

    /// @brief Destructor - deallocates all nodes including nil
    /// MEMORY SAFETY: Ensures all allocated nodes are properly freed
    ~Map() {
        if (nil_) {
            clear();  // Deallocate all data nodes
            alloc_.deallocate(nil_);  // Finally deallocate sentinel
        }
    }

    /// @brief Copy constructor - deep copies all elements
    /// @param other Map to copy from
    /// MEMORY: Allocates new nodes for all elements
    Map(const Map& other) 
        : root_(nullptr), size_(0), comp_(other.comp_), alloc_(other.alloc_) {
        initialize_nil();  // Create our own nil node
        root_ = nil_;
        // Deep copy all elements
        for (const auto& item : other) {
            insert(item);  // Each insert allocates a new node
        }
    }

    /// @brief Copy assignment - replaces contents with copy of other
    /// @param other Map to copy from
    /// MEMORY SAFETY: Clears existing nodes before copying
    Map& operator=(const Map& other) {
        if (this != &other) {
            clear();  // Deallocate all existing nodes (except nil)
            comp_ = other.comp_;
            // Deep copy all elements
            for (const auto& item : other) {
                insert(item);  // Allocate new node for each element
            }
        }
        return *this;
    }

    /// @brief Move constructor - transfers ownership of nodes
    /// @param other Map to move from
    /// MEMORY: Takes ownership of all nodes from other
    /// WARNING: Leaves other in moved-from state (nil_ = nullptr)
    Map(Map&& other) noexcept
        : root_(other.root_), nil_(other.nil_), size_(other.size_),
          comp_(std::move(other.comp_)), alloc_(other.alloc_) {
        // Leave other in valid but empty state
        other.root_ = nullptr;
        other.nil_ = nullptr;  // IMPORTANT: other's destructor must check for null
        other.size_ = 0;
    }

    Map& operator=(Map&& other) noexcept = delete;  // Can't reassign allocator reference

    iterator begin() {
        if (root_ == nil_) return end();
        Node* leftmost = root_;
        while (leftmost->left != nil_) {
            leftmost = leftmost->left;
        }
        return iterator(leftmost, nil_);
    }

    const_iterator begin() const {
        if (root_ == nil_) return end();
        const Node* leftmost = root_;
        while (leftmost->left != nil_) {
            leftmost = leftmost->left;
        }
        return const_iterator(leftmost, nil_);
    }

    iterator end() { return iterator(nil_, nil_); }
    const_iterator end() const { return const_iterator(nil_, nil_); }

    bool empty() const { return size_ == 0; }
    size_type size() const { return size_; }

    /// @brief Remove all elements from the map
    /// MEMORY: Deallocates all nodes except nil (which persists)
    void clear() {
        if (nil_) {  // Check needed for moved-from maps
            destroy_tree(root_);  // Recursively deallocate all data nodes
            root_ = nil_;         // Reset to empty tree
            size_ = 0;
            update_nil_pointers();
        }
    }

    /// @brief Insert a key-value pair into the map
    /// @param value Pair to insert
    /// @return Pair of iterator to element and bool (true if inserted)
    /// MEMORY: Allocates new node only if key doesn't exist
    std::pair<iterator, bool> insert(const value_type& value) {
        Node* y = nil_;  // Parent of x
        Node* x = root_; // Current node
        
        // Find insertion point
        while (x != nil_) {
            y = x;
            if (comp_(value.first, x->data.first)) {
                x = x->left;
            } else if (comp_(x->data.first, value.first)) {
                x = x->right;
            } else {
                // Key already exists, no allocation needed
                return {iterator(x, nil_), false};
            }
        }

        // Allocate and construct new node
        Node* z = alloc_.allocate(value);
        if (!z) {
            // Allocation failed
            return {iterator(nil_, nil_), false};
        }
        z->parent = y;
        z->left = nil_;
        z->right = nil_;
        z->color = Color::RED;

        if (y == nil_) {
            root_ = z;
        } else if (comp_(z->data.first, y->data.first)) {
            y->left = z;
        } else {
            y->right = z;
        }

        insert_fixup(z);
        ++size_;
        update_nil_pointers();
        return {iterator(z, nil_), true};
    }

    /// @brief Construct element in-place
    /// @param args Arguments to forward to value_type constructor
    /// MEMORY LEAK CHECK: Properly deallocates temp node if key exists
    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        // Allocate temp node to get the key for comparison
        Node* temp = alloc_.allocate(std::forward<Args>(args)...);
        if (!temp) {
            // Allocation failed
            return {iterator(nil_, nil_), false};
        }
        
        Node* y = nil_;
        Node* x = root_;
        
        // Find insertion point
        while (x != nil_) {
            y = x;
            if (comp_(temp->data.first, x->data.first)) {
                x = x->left;
            } else if (comp_(x->data.first, temp->data.first)) {
                x = x->right;
            } else {
                // Key exists - MUST deallocate temp to prevent leak
                alloc_.deallocate(temp);  // Return memory to pool
                return {iterator(x, nil_), false};
            }
        }

        temp->parent = y;
        temp->left = nil_;
        temp->right = nil_;
        temp->color = Color::RED;

        if (y == nil_) {
            root_ = temp;
        } else if (comp_(temp->data.first, y->data.first)) {
            y->left = temp;
        } else {
            y->right = temp;
        }

        insert_fixup(temp);
        ++size_;
        update_nil_pointers();
        return {iterator(temp, nil_), true};
    }

    iterator find(const Key& key) {
        Node* current = root_;
        while (current != nil_) {
            if (comp_(key, current->data.first)) {
                current = current->left;
            } else if (comp_(current->data.first, key)) {
                current = current->right;
            } else {
                return iterator(current, nil_);
            }
        }
        return end();
    }

    const_iterator find(const Key& key) const {
        const Node* current = root_;
        while (current != nil_) {
            if (comp_(key, current->data.first)) {
                current = current->left;
            } else if (comp_(current->data.first, key)) {
                current = current->right;
            } else {
                return const_iterator(current, nil_);
            }
        }
        return end();
    }

    /// @brief Access or insert element with given key
    /// @param key Key to access/insert
    /// MEMORY: Allocates new node if key doesn't exist
    T& operator[](const Key& key) {
        auto result = insert({key, T{}});  // Insert default value if not found
        return result.first->second;
    }

    size_type erase(const Key& key) {
        auto it = find(key);
        if (it == end()) {
            return 0;
        }
        erase(it);
        return 1;
    }

    /// @brief Erase element at iterator position
    /// @param pos Iterator to element to erase
    /// @return Iterator to next element
    /// MEMORY: Deallocates the erased node
    iterator erase(iterator pos) {
        Node* z = pos.node_;  // Node to delete
        Node* y = z;          // Node that might violate RB properties
        Node* x;              // Node that replaces y
        Color y_original_color = y->color;
        
        iterator next = pos;
        ++next;  // Get next element before deletion

        if (z->left == nil_) {
            x = z->right;
            transplant(z, z->right);
        } else if (z->right == nil_) {
            x = z->left;
            transplant(z, z->left);
        } else {
            y = minimum(z->right);
            y_original_color = y->color;
            x = y->right;
            if (y->parent == z) {
                x->parent = y;
            } else {
                transplant(y, y->right);
                y->right = z->right;
                y->right->parent = y;
            }
            transplant(z, y);
            y->left = z->left;
            y->left->parent = y;
            y->color = z->color;
        }

        if (y_original_color == Color::BLACK) {
            delete_fixup(x);
        }

        // MEMORY CLEANUP: Deallocate the removed node
        alloc_.deallocate(z);  // Return memory to pool (destructor called by pool)
        --size_;
        update_nil_pointers();

        return next;
    }

    size_type count(const Key& key) const {
        return find(key) != end() ? 1 : 0;
    }
};

}
