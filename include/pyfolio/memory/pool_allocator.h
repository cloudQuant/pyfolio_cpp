#pragma once

#include "../core/types.h"
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace pyfolio::memory {

/**
 * @brief High-performance memory pool allocator for financial data structures
 *
 * This allocator is designed for high-frequency trading scenarios where
 * allocation speed is critical and memory fragmentation must be minimized.
 *
 * Features:
 * - O(1) allocation and deallocation
 * - Thread-safe operations
 * - Memory alignment support
 * - Configurable pool sizes
 * - Memory usage tracking
 * - Automatic growth when needed
 */

/**
 * @brief Memory pool statistics
 */
struct PoolStats {
    size_t total_allocated_bytes;  // Total memory allocated from system
    size_t used_bytes;             // Currently used memory
    size_t free_bytes;             // Available memory in pools
    size_t num_allocations;        // Total allocation count
    size_t num_deallocations;      // Total deallocation count
    size_t num_pools;              // Number of memory pools
    size_t largest_allocation;     // Largest single allocation
    double fragmentation_ratio;    // Fragmentation percentage

    /**
     * @brief Get memory efficiency percentage
     */
    double efficiency() const noexcept {
        if (total_allocated_bytes == 0)
            return 100.0;
        return 100.0 * static_cast<double>(used_bytes) / total_allocated_bytes;
    }

    /**
     * @brief Check if pool needs cleanup
     */
    bool needs_cleanup() const noexcept { return fragmentation_ratio > 50.0 || efficiency() < 70.0; }
};

/**
 * @brief Fixed-size block allocator for specific types
 */
template <typename T, size_t BlockCount = 1024>
class FixedBlockAllocator {
  public:
    static constexpr size_t block_size = sizeof(T);
    static constexpr size_t alignment  = alignof(T);
    static constexpr size_t pool_size  = BlockCount * block_size;

  private:
    struct Block {
        alignas(T) char data[block_size];
        Block* next;
    };

    std::unique_ptr<char[]> memory_pool_;
    Block* free_list_;
    std::atomic<size_t> allocated_count_;
    std::atomic<size_t> total_allocations_;
    std::mutex mutex_;

  public:
    FixedBlockAllocator()
        : memory_pool_(std::make_unique<char[]>(pool_size)), free_list_(nullptr), allocated_count_(0),
          total_allocations_(0) {
        initialize_free_list();
    }

    ~FixedBlockAllocator() = default;

    // Non-copyable, non-movable for thread safety
    FixedBlockAllocator(const FixedBlockAllocator&)            = delete;
    FixedBlockAllocator& operator=(const FixedBlockAllocator&) = delete;
    FixedBlockAllocator(FixedBlockAllocator&&)                 = delete;
    FixedBlockAllocator& operator=(FixedBlockAllocator&&)      = delete;

    /**
     * @brief Allocate a block for type T
     */
    T* allocate() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!free_list_) {
            return nullptr;  // Pool exhausted
        }

        Block* block = free_list_;
        free_list_   = free_list_->next;

        ++allocated_count_;
        ++total_allocations_;

        return reinterpret_cast<T*>(block);
    }

    /**
     * @brief Deallocate a block
     */
    void deallocate(T* ptr) {
        if (!ptr)
            return;

        std::lock_guard<std::mutex> lock(mutex_);

        Block* block = reinterpret_cast<Block*>(ptr);
        block->next  = free_list_;
        free_list_   = block;

        --allocated_count_;
    }

    /**
     * @brief Check if pointer belongs to this pool
     */
    bool owns(const T* ptr) const noexcept {
        const char* char_ptr   = reinterpret_cast<const char*>(ptr);
        const char* pool_start = memory_pool_.get();
        const char* pool_end   = pool_start + pool_size;

        return char_ptr >= pool_start && char_ptr < pool_end;
    }

    /**
     * @brief Get allocation statistics
     */
    PoolStats get_stats() const noexcept {
        PoolStats stats{};
        stats.total_allocated_bytes = pool_size;
        stats.used_bytes            = allocated_count_.load() * block_size;
        stats.free_bytes            = (BlockCount - allocated_count_.load()) * block_size;
        stats.num_allocations       = total_allocations_.load();
        stats.num_deallocations     = total_allocations_.load() - allocated_count_.load();
        stats.num_pools             = 1;
        stats.largest_allocation    = block_size;
        stats.fragmentation_ratio   = 0.0;  // Fixed-size blocks don't fragment

        return stats;
    }

    /**
     * @brief Get available blocks count
     */
    size_t available_blocks() const noexcept { return BlockCount - allocated_count_.load(); }

    /**
     * @brief Check if pool is full
     */
    bool is_full() const noexcept { return allocated_count_.load() >= BlockCount; }

  private:
    void initialize_free_list() {
        char* ptr = memory_pool_.get();

        for (size_t i = 0; i < BlockCount; ++i) {
            Block* block = reinterpret_cast<Block*>(ptr + i * block_size);
            block->next  = (i == BlockCount - 1) ? nullptr : reinterpret_cast<Block*>(ptr + (i + 1) * block_size);
        }

        free_list_ = reinterpret_cast<Block*>(memory_pool_.get());
    }
};

/**
 * @brief Variable-size memory pool allocator
 */
class VariablePoolAllocator {
  public:
    static constexpr size_t default_pool_size   = 1024 * 1024;  // 1MB pools
    static constexpr size_t max_allocation_size = 64 * 1024;    // 64KB max allocation
    static constexpr size_t alignment           = 16;           // 16-byte alignment for SIMD

  private:
    struct Block {
        size_t size;
        Block* next;

        char* data() { return reinterpret_cast<char*>(this + 1); }

        const char* data() const { return reinterpret_cast<const char*>(this + 1); }
    };

    struct Pool {
        std::unique_ptr<char[]> memory;
        size_t size;
        size_t used;
        Block* free_list;

        Pool(size_t pool_size)
            : memory(std::make_unique<char[]>(pool_size)), size(pool_size), used(0), free_list(nullptr) {
            // Initialize with one large free block
            free_list       = reinterpret_cast<Block*>(memory.get());
            free_list->size = pool_size - sizeof(Block);
            free_list->next = nullptr;
        }
    };

    std::vector<std::unique_ptr<Pool>> pools_;
    size_t pool_size_;
    std::atomic<size_t> total_allocated_;
    std::atomic<size_t> total_used_;
    std::atomic<size_t> num_allocations_;
    std::atomic<size_t> num_deallocations_;
    std::mutex mutex_;

  public:
    explicit VariablePoolAllocator(size_t pool_size = default_pool_size)
        : pool_size_(pool_size), total_allocated_(0), total_used_(0), num_allocations_(0), num_deallocations_(0) {
        // Create initial pool
        add_pool();
    }

    ~VariablePoolAllocator() = default;

    // Non-copyable, non-movable
    VariablePoolAllocator(const VariablePoolAllocator&)            = delete;
    VariablePoolAllocator& operator=(const VariablePoolAllocator&) = delete;
    VariablePoolAllocator(VariablePoolAllocator&&)                 = delete;
    VariablePoolAllocator& operator=(VariablePoolAllocator&&)      = delete;

    /**
     * @brief Allocate memory of specified size
     */
    void* allocate(size_t size, size_t align = alignment) {
        if (size == 0 || size > max_allocation_size) {
            return nullptr;
        }

        // Align size
        size_t aligned_size = align_size(size, align);
        size_t total_size   = sizeof(Block) + aligned_size;

        std::lock_guard<std::mutex> lock(mutex_);

        // Find suitable block
        for (auto& pool : pools_) {
            if (void* ptr = allocate_from_pool(*pool, total_size)) {
                total_used_ += total_size;
                ++num_allocations_;
                return ptr;
            }
        }

        // Need new pool
        add_pool();
        if (void* ptr = allocate_from_pool(*pools_.back(), total_size)) {
            total_used_ += total_size;
            ++num_allocations_;
            return ptr;
        }

        return nullptr;  // Failed to allocate
    }

    /**
     * @brief Deallocate memory
     */
    void deallocate(void* ptr) {
        if (!ptr)
            return;

        std::lock_guard<std::mutex> lock(mutex_);

        // Find which pool owns this pointer
        for (auto& pool : pools_) {
            if (owns_pointer(*pool, ptr)) {
                deallocate_from_pool(*pool, ptr);
                ++num_deallocations_;
                return;
            }
        }

        // Pointer not from our pools - this is an error
        assert(false && "Attempting to deallocate pointer not from pool");
    }

    /**
     * @brief Get allocation statistics
     */
    PoolStats get_stats() const noexcept {
        PoolStats stats{};
        stats.total_allocated_bytes = total_allocated_.load();
        stats.used_bytes            = total_used_.load();
        stats.free_bytes            = stats.total_allocated_bytes - stats.used_bytes;
        stats.num_allocations       = num_allocations_.load();
        stats.num_deallocations     = num_deallocations_.load();
        stats.num_pools             = pools_.size();
        stats.largest_allocation    = max_allocation_size;

        if (stats.total_allocated_bytes > 0) {
            stats.fragmentation_ratio =
                100.0 * (1.0 - static_cast<double>(stats.used_bytes) / stats.total_allocated_bytes);
        }

        return stats;
    }

    /**
     * @brief Cleanup fragmented memory
     */
    void defragment() {
        std::lock_guard<std::mutex> lock(mutex_);

        // Coalesce free blocks in each pool
        for (auto& pool : pools_) {
            coalesce_free_blocks(*pool);
        }
    }

  private:
    void add_pool() {
        auto pool = std::make_unique<Pool>(pool_size_);
        total_allocated_ += pool_size_;
        pools_.push_back(std::move(pool));
    }

    void* allocate_from_pool(Pool& pool, size_t size) {
        Block** current = &pool.free_list;

        while (*current) {
            Block* block = *current;

            if (block->size >= size) {
                // Remove from free list
                *current = block->next;

                // Split block if it's much larger
                if (block->size > size + sizeof(Block) + 32) {
                    Block* new_block = reinterpret_cast<Block*>(reinterpret_cast<char*>(block) + sizeof(Block) + size);
                    new_block->size  = block->size - size - sizeof(Block);
                    new_block->next  = *current;
                    *current         = new_block;

                    block->size = size;
                }

                block->next = nullptr;  // Mark as allocated
                pool.used += sizeof(Block) + block->size;

                return block->data();
            }

            current = &(block->next);
        }

        return nullptr;
    }

    void deallocate_from_pool(Pool& pool, void* ptr) {
        Block* block = reinterpret_cast<Block*>(static_cast<char*>(ptr) - sizeof(Block));

        // Add to free list
        block->next    = pool.free_list;
        pool.free_list = block;

        pool.used -= sizeof(Block) + block->size;
        total_used_ -= sizeof(Block) + block->size;
    }

    bool owns_pointer(const Pool& pool, const void* ptr) const {
        const char* char_ptr   = static_cast<const char*>(ptr);
        const char* pool_start = pool.memory.get();
        const char* pool_end   = pool_start + pool.size;

        return char_ptr >= pool_start && char_ptr < pool_end;
    }

    void coalesce_free_blocks(Pool& pool) {
        // Sort free blocks by address
        std::vector<Block*> free_blocks;
        Block* current = pool.free_list;

        while (current) {
            free_blocks.push_back(current);
            current = current->next;
        }

        if (free_blocks.size() < 2)
            return;

        std::sort(free_blocks.begin(), free_blocks.end());

        // Coalesce adjacent blocks
        pool.free_list = nullptr;
        Block* prev    = nullptr;

        for (Block* block : free_blocks) {
            if (prev &&
                (reinterpret_cast<char*>(prev) + sizeof(Block) + prev->size == reinterpret_cast<char*>(block))) {
                // Merge with previous block
                prev->size += sizeof(Block) + block->size;
            } else {
                // Add to free list
                if (prev) {
                    prev->next = block;
                } else {
                    pool.free_list = block;
                }
                prev = block;
            }
        }

        if (prev) {
            prev->next = nullptr;
        }
    }

    static size_t align_size(size_t size, size_t align) { return (size + align - 1) & ~(align - 1); }
};

/**
 * @brief STL-compatible allocator using memory pools
 */
template <typename T>
class PoolAllocator {
  private:
    static FixedBlockAllocator<T>& get_allocator() {
        static FixedBlockAllocator<T> allocator;
        return allocator;
    }

  public:
    using value_type      = T;
    using pointer         = T*;
    using const_pointer   = const T*;
    using reference       = T&;
    using const_reference = const T&;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;

    template <typename U>
    struct rebind {
        using other = PoolAllocator<U>;
    };

    PoolAllocator() noexcept = default;

    template <typename U>
    PoolAllocator(const PoolAllocator<U>&) noexcept {}

    pointer allocate(size_type n) {
        if (n != 1) {
            throw std::bad_alloc();  // Only support single object allocation
        }

        pointer ptr = get_allocator().allocate();
        if (!ptr) {
            throw std::bad_alloc();
        }

        return ptr;
    }

    void deallocate(pointer ptr, size_type) { get_allocator().deallocate(ptr); }

    template <typename U>
    bool operator==(const PoolAllocator<U>&) const noexcept {
        return true;  // All instances are equivalent
    }

    template <typename U>
    bool operator!=(const PoolAllocator<U>&) const noexcept {
        return false;
    }

    /**
     * @brief Get allocator statistics
     */
    static PoolStats get_stats() { return get_allocator().get_stats(); }
};

/**
 * @brief Type aliases for pool-allocated containers
 */
template <typename T>
using pool_vector = std::vector<T, PoolAllocator<T>>;

template <typename Key, typename Value>
using pool_map = std::map<Key, Value, std::less<Key>, PoolAllocator<std::pair<const Key, Value>>>;

/**
 * @brief Global memory pool manager
 */
class MemoryPoolManager {
  private:
    VariablePoolAllocator variable_pool_;
    std::mutex stats_mutex_;

  public:
    static MemoryPoolManager& instance() {
        static MemoryPoolManager manager;
        return manager;
    }

    void* allocate(size_t size, size_t alignment = 16) { return variable_pool_.allocate(size, alignment); }

    void deallocate(void* ptr) { variable_pool_.deallocate(ptr); }

    PoolStats get_global_stats() {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return variable_pool_.get_stats();
    }

    void defragment() { variable_pool_.defragment(); }

  private:
    MemoryPoolManager() = default;
};

/**
 * @brief RAII memory management utilities
 */
template <typename T>
class PoolPtr {
  private:
    T* ptr_;

  public:
    explicit PoolPtr(T* ptr = nullptr) : ptr_(ptr) {}

    ~PoolPtr() {
        if (ptr_) {
            ptr_->~T();
            MemoryPoolManager::instance().deallocate(ptr_);
        }
    }

    // Move-only semantics
    PoolPtr(const PoolPtr&)            = delete;
    PoolPtr& operator=(const PoolPtr&) = delete;

    PoolPtr(PoolPtr&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }

    PoolPtr& operator=(PoolPtr&& other) noexcept {
        if (this != &other) {
            if (ptr_) {
                ptr_->~T();
                MemoryPoolManager::instance().deallocate(ptr_);
            }
            ptr_       = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    T* get() const noexcept { return ptr_; }
    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }

    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    T* release() noexcept {
        T* temp = ptr_;
        ptr_    = nullptr;
        return temp;
    }

    void reset(T* ptr = nullptr) {
        if (ptr_) {
            ptr_->~T();
            MemoryPoolManager::instance().deallocate(ptr_);
        }
        ptr_ = ptr;
    }
};

/**
 * @brief Factory function for pool-allocated objects
 */
template <typename T, typename... Args>
PoolPtr<T> make_pool_ptr(Args&&... args) {
    void* memory = MemoryPoolManager::instance().allocate(sizeof(T), alignof(T));
    if (!memory) {
        throw std::bad_alloc();
    }

    try {
        T* ptr = new (memory) T(std::forward<Args>(args)...);
        return PoolPtr<T>(ptr);
    } catch (...) {
        MemoryPoolManager::instance().deallocate(memory);
        throw;
    }
}

}  // namespace pyfolio::memory