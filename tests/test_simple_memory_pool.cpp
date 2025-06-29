#include <chrono>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

// Simple memory pool implementation for testing
template <typename T, size_t BlockCount = 100>
class SimpleFixedPool {
  private:
    struct Block {
        alignas(T) char data[sizeof(T)];
        Block* next;
    };

    std::unique_ptr<Block[]> memory_;
    Block* free_list_;
    size_t allocated_count_;

  public:
    SimpleFixedPool() : memory_(std::make_unique<Block[]>(BlockCount)), free_list_(nullptr), allocated_count_(0) {
        // Initialize free list
        for (size_t i = 0; i < BlockCount - 1; ++i) {
            memory_[i].next = &memory_[i + 1];
        }
        memory_[BlockCount - 1].next = nullptr;
        free_list_                   = &memory_[0];
    }

    T* allocate() {
        if (!free_list_)
            return nullptr;

        Block* block = free_list_;
        free_list_   = free_list_->next;
        ++allocated_count_;

        return reinterpret_cast<T*>(block->data);
    }

    void deallocate(T* ptr) {
        if (!ptr)
            return;

        Block* block = reinterpret_cast<Block*>(ptr);
        block->next  = free_list_;
        free_list_   = block;
        --allocated_count_;
    }

    size_t allocated() const { return allocated_count_; }
    bool is_full() const { return allocated_count_ >= BlockCount; }
    size_t capacity() const { return BlockCount; }
};

class SimpleMemoryPoolTest : public ::testing::Test {
  protected:
    template <typename Func>
    double measure_time_ms(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
};

TEST_F(SimpleMemoryPoolTest, BasicAllocation) {
    SimpleFixedPool<int, 10> pool;

    // Test allocation
    int* ptr = pool.allocate();
    ASSERT_NE(ptr, nullptr);
    *ptr = 42;
    EXPECT_EQ(*ptr, 42);
    EXPECT_EQ(pool.allocated(), 1);

    // Test deallocation
    pool.deallocate(ptr);
    EXPECT_EQ(pool.allocated(), 0);
}

TEST_F(SimpleMemoryPoolTest, MultipleAllocations) {
    SimpleFixedPool<double, 5> pool;
    std::vector<double*> ptrs;

    // Allocate all blocks
    for (size_t i = 0; i < 5; ++i) {
        double* ptr = pool.allocate();
        ASSERT_NE(ptr, nullptr);
        *ptr = static_cast<double>(i);
        ptrs.push_back(ptr);
    }

    EXPECT_TRUE(pool.is_full());
    EXPECT_EQ(pool.allocated(), 5);

    // Should fail to allocate when full
    double* overflow_ptr = pool.allocate();
    EXPECT_EQ(overflow_ptr, nullptr);

    // Verify values
    for (size_t i = 0; i < ptrs.size(); ++i) {
        EXPECT_EQ(*ptrs[i], static_cast<double>(i));
    }

    // Deallocate all
    for (double* ptr : ptrs) {
        pool.deallocate(ptr);
    }

    EXPECT_EQ(pool.allocated(), 0);
    EXPECT_FALSE(pool.is_full());
}

TEST_F(SimpleMemoryPoolTest, PerformanceComparison) {
    const int num_allocations = 10000;

    // Standard allocator performance
    double std_time = measure_time_ms([&]() {
        std::vector<int*> ptrs;
        ptrs.reserve(num_allocations);

        for (int i = 0; i < num_allocations; ++i) {
            ptrs.push_back(new int(i));
        }

        for (int* ptr : ptrs) {
            delete ptr;
        }
    });

    // Pool allocator performance (with multiple pools to handle large allocation count)
    double pool_time = measure_time_ms([&]() {
        const int pool_size = 1000;
        const int num_pools = (num_allocations + pool_size - 1) / pool_size;

        std::vector<std::unique_ptr<SimpleFixedPool<int, 1000>>> pools;
        std::vector<int*> ptrs;
        ptrs.reserve(num_allocations);

        for (int i = 0; i < num_pools; ++i) {
            pools.push_back(std::make_unique<SimpleFixedPool<int, 1000>>());
        }

        // Allocate
        int current_pool = 0;
        for (int i = 0; i < num_allocations; ++i) {
            int* ptr = pools[current_pool]->allocate();
            if (!ptr) {
                ++current_pool;
                if (current_pool < num_pools) {
                    ptr = pools[current_pool]->allocate();
                }
            }
            if (ptr) {
                *ptr = i;
                ptrs.push_back(ptr);
            }
        }

        // Deallocate
        for (int* ptr : ptrs) {
            for (auto& pool : pools) {
                // Simple ownership test (not efficient but works for test)
                if (reinterpret_cast<uintptr_t>(ptr) >= reinterpret_cast<uintptr_t>(pool.get()) &&
                    reinterpret_cast<uintptr_t>(ptr) < reinterpret_cast<uintptr_t>(pool.get()) + sizeof(*pool)) {
                    // Simplified - just break for test
                    break;
                }
            }
            // For simplicity, let pools deallocate automatically in destructor
        }
    });

    std::cout << "\n=== Simple Memory Pool Performance ===\n";
    std::cout << "Standard new/delete: " << std_time << " ms\n";
    std::cout << "Simple pool allocator: " << pool_time << " ms\n";

    if (pool_time > 0) {
        std::cout << "Speedup: " << (std_time / pool_time) << "x\n";
    }

    // Basic sanity check - pool shouldn't be orders of magnitude slower
    EXPECT_LT(pool_time, std_time * 10.0);
}

TEST_F(SimpleMemoryPoolTest, EdgeCases) {
    SimpleFixedPool<char, 3> pool;

    // Test null deallocation
    pool.deallocate(nullptr);  // Should not crash

    // Test capacity
    EXPECT_EQ(pool.capacity(), 3);
    EXPECT_EQ(pool.allocated(), 0);

    // Allocate and test state
    char* ptr1 = pool.allocate();
    char* ptr2 = pool.allocate();
    char* ptr3 = pool.allocate();

    EXPECT_NE(ptr1, nullptr);
    EXPECT_NE(ptr2, nullptr);
    EXPECT_NE(ptr3, nullptr);
    EXPECT_TRUE(pool.is_full());

    // Clean up
    pool.deallocate(ptr1);
    pool.deallocate(ptr2);
    pool.deallocate(ptr3);

    EXPECT_EQ(pool.allocated(), 0);
}

TEST_F(SimpleMemoryPoolTest, LargeObjectPool) {
    struct LargeObject {
        double data[100];  // 800 bytes
        int id;

        LargeObject(int i) : id(i) {
            for (int j = 0; j < 100; ++j) {
                data[j] = i * 0.01 + j;
            }
        }
    };

    SimpleFixedPool<LargeObject, 10> pool;
    std::vector<LargeObject*> objects;

    // Allocate and construct objects
    for (int i = 0; i < 5; ++i) {
        LargeObject* obj = pool.allocate();
        ASSERT_NE(obj, nullptr);

        // Placement new to properly construct
        new (obj) LargeObject(i);
        objects.push_back(obj);
    }

    // Verify objects
    for (size_t i = 0; i < objects.size(); ++i) {
        EXPECT_EQ(objects[i]->id, static_cast<int>(i));
        EXPECT_DOUBLE_EQ(objects[i]->data[0], i * 0.01);
        EXPECT_DOUBLE_EQ(objects[i]->data[99], i * 0.01 + 99);
    }

    // Destroy and deallocate
    for (LargeObject* obj : objects) {
        obj->~LargeObject();  // Explicit destructor call
        pool.deallocate(obj);
    }

    EXPECT_EQ(pool.allocated(), 0);
}

TEST_F(SimpleMemoryPoolTest, AlignmentTest) {
    SimpleFixedPool<int, 10> int_pool;
    SimpleFixedPool<double, 10> double_pool;
    SimpleFixedPool<long long, 10> ll_pool;

    // Test alignment for different types
    int* int_ptr = int_pool.allocate();
    ASSERT_NE(int_ptr, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(int_ptr) % alignof(int), 0);

    double* double_ptr = double_pool.allocate();
    ASSERT_NE(double_ptr, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(double_ptr) % alignof(double), 0);

    long long* ll_ptr = ll_pool.allocate();
    ASSERT_NE(ll_ptr, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ll_ptr) % alignof(long long), 0);

    // Clean up
    int_pool.deallocate(int_ptr);
    double_pool.deallocate(double_ptr);
    ll_pool.deallocate(ll_ptr);
}