#pragma once

#include <cstddef>
#include <new>
#include "strategy.hpp"
#include "../bump_allocator.hpp"

namespace ftl::allocator {

namespace detail {

// Non-virtual bump pool implementation to avoid code duplication
// This handles the free list management and bump allocation
class BumpPoolImpl {
private:
    // Base node structure for the free list
    struct NodeBase {
        NodeBase* next{nullptr};
    };
    
    BumpAllocator& allocator_;
    NodeBase* free_list_{nullptr};
    std::size_t node_size_;      // Total size of node including header and storage
    std::size_t storage_offset_; // Offset from node start to storage
    std::size_t alignment_;      // Required alignment
    
public:
    BumpPoolImpl(BumpAllocator& allocator, std::size_t size, std::size_t alignment) noexcept
        : allocator_(allocator), alignment_(alignment) {
        // Calculate storage offset to ensure proper alignment
        // Storage comes after NodeBase, aligned to requested alignment
        storage_offset_ = sizeof(NodeBase);
        if (alignment > alignof(NodeBase)) {
            storage_offset_ = (storage_offset_ + alignment - 1) & ~(alignment - 1);
        }
        node_size_ = storage_offset_ + size;
    }
    
    void* allocate() noexcept {
        // Try to get from free list first
        if (free_list_) {
            NodeBase* node = free_list_;
            free_list_ = node->next;
            // Return pointer to storage area
            return reinterpret_cast<unsigned char*>(node) + storage_offset_;
        }
        
        // Allocate new from bump allocator
        // Request alignment for the whole node to ensure storage is aligned
        void* mem = allocator_.allocate(node_size_, alignment_);
        if (!mem) return nullptr;
        
        // Return pointer to storage area
        return reinterpret_cast<unsigned char*>(mem) + storage_offset_;
    }
    
    void deallocate(void* ptr) noexcept {
        if (!ptr) return;
        
        // Calculate Node address from storage pointer
        NodeBase* node = reinterpret_cast<NodeBase*>(
            static_cast<unsigned char*>(ptr) - storage_offset_
        );
        
        // Add to free list
        node->next = free_list_;
        free_list_ = node;
    }
};

} // namespace detail

// Bump pool-based block allocation strategy
class BumpPoolBlockStrategy : public IBlockStrategy {
private:
    detail::BumpPoolImpl impl_;
    
public:
    BumpPoolBlockStrategy(BumpAllocator& allocator, std::size_t size, 
                         std::size_t alignment = alignof(std::max_align_t)) noexcept
        : impl_(allocator, size, alignment) {}
    
    void* allocate() noexcept override {
        return impl_.allocate();
    }
    
    void deallocate(void* ptr) noexcept override {
        impl_.deallocate(ptr);
    }
};

// Bump pool-based object allocation strategy
template <typename T>
class BumpPoolObjStrategy : public IObjStrategy<T> {
private:
    detail::BumpPoolImpl impl_;
    
public:
    explicit BumpPoolObjStrategy(BumpAllocator& allocator) noexcept
        : impl_(allocator, sizeof(T), alignof(T)) {}
    
    void* allocate() noexcept override {
        return impl_.allocate();
    }
    
    void deallocate(void* ptr) noexcept override {
        impl_.deallocate(ptr);
    }
};

} // namespace ftl::allocator