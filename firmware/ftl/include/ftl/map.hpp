#ifndef FTL_MAP_HPP
#define FTL_MAP_HPP

#include <functional>
#include <iterator>
#include <utility>
#include <cassert>
#include <cstddef>
#include "ftl/bump_pool_allocator2.hpp"

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
    enum class Color : bool { RED = false, BLACK = true };

public:
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
    Node* root_;
    Node* nil_;
    size_type size_;
    Compare comp_;
    BumpPoolAllocator2& alloc_;

    void initialize_nil() {
        nil_ = alloc_.allocate<Node>();
        nil_->parent = nil_->left = nil_->right = nil_;
        nil_->color = Color::BLACK;
    }

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

    Node* minimum(Node* x) const {
        while (x->left != nil_) {
            x = x->left;
        }
        return x;
    }

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

    void destroy_tree(Node* node) {
        if (node != nil_) {
            destroy_tree(node->left);
            destroy_tree(node->right);
            node->~Node();
            alloc_.deallocate<Node>(node);
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
            if (node_->left != nil_) {
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
            if (node_->left != nil_) {
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

    explicit Map(BumpPoolAllocator2& alloc, const Compare& comp = Compare())
        : root_(nullptr), size_(0), comp_(comp), alloc_(alloc) {
        initialize_nil();
        root_ = nil_;
    }

    ~Map() {
        clear();
        if (nil_) {
            alloc_.deallocate<Node>(nil_);
        }
    }

    Map(const Map& other) 
        : root_(nullptr), size_(0), comp_(other.comp_), alloc_(other.alloc_) {
        initialize_nil();
        root_ = nil_;
        for (const auto& item : other) {
            insert(item);
        }
    }

    Map& operator=(const Map& other) {
        if (this != &other) {
            clear();
            comp_ = other.comp_;
            for (const auto& item : other) {
                insert(item);
            }
        }
        return *this;
    }

    Map(Map&& other) noexcept
        : root_(other.root_), nil_(other.nil_), size_(other.size_),
          comp_(std::move(other.comp_)), alloc_(other.alloc_) {
        other.root_ = nullptr;
        other.nil_ = nullptr;
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

    void clear() {
        destroy_tree(root_);
        root_ = nil_;
        size_ = 0;
    }

    std::pair<iterator, bool> insert(const value_type& value) {
        Node* y = nil_;
        Node* x = root_;
        
        while (x != nil_) {
            y = x;
            if (comp_(value.first, x->data.first)) {
                x = x->left;
            } else if (comp_(x->data.first, value.first)) {
                x = x->right;
            } else {
                return {iterator(x, nil_), false};
            }
        }

        Node* z = alloc_.allocate<Node>();
        new (z) Node(value);
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
        return {iterator(z, nil_), true};
    }

    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        Node* temp = alloc_.allocate<Node>();
        new (temp) Node(std::forward<Args>(args)...);
        
        Node* y = nil_;
        Node* x = root_;
        
        while (x != nil_) {
            y = x;
            if (comp_(temp->data.first, x->data.first)) {
                x = x->left;
            } else if (comp_(x->data.first, temp->data.first)) {
                x = x->right;
            } else {
                temp->~Node();
                alloc_.deallocate<Node>(temp);
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

    T& operator[](const Key& key) {
        auto result = insert({key, T{}});
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

    iterator erase(iterator pos) {
        Node* z = pos.node_;
        Node* y = z;
        Node* x;
        Color y_original_color = y->color;
        
        iterator next = pos;
        ++next;

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

        z->~Node();
        alloc_.deallocate<Node>(z);
        --size_;

        return next;
    }

    size_type count(const Key& key) const {
        return find(key) != end() ? 1 : 0;
    }
};

}

#endif