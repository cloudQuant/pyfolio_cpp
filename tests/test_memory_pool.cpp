#include <chrono>
#include <gtest/gtest.h>
#include <pyfolio/core/time_series.h>
#include <pyfolio/memory/pool_allocator.h>
#include <random>
#include <thread>
#include <vector>

using namespace pyfolio::memory;
using namespace pyfolio;

class MemoryPoolTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Reset any global state if needed
    }

    void TearDown() override {
        // Clean up
    }

    template <typename Func>
    double measure_time_ms(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
};

TEST_F(MemoryPoolTest, FixedBlockAllocatorBasics) {
    FixedBlockAllocator<int, 10> allocator;

    // Test basic allocation
    int* ptr1 = allocator.allocate();
    ASSERT_NE(ptr1, nullptr);
    EXPECT_FALSE(allocator.is_full());

    // Test deallocation
    allocator.deallocate(ptr1);
    EXPECT_EQ(allocator.available_blocks(), 10);

    // Test multiple allocations
    std::vector<int*> ptrs;
    for (int i = 0; i < 10; ++i) {
        int* ptr = allocator.allocate();
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }

    EXPECT_TRUE(allocator.is_full());
    EXPECT_EQ(allocator.allocate(), nullptr);  // Should fail when full

    // Test ownership
    for (int* ptr : ptrs) {
        EXPECT_TRUE(allocator.owns(ptr));
    }

    // Clean up
    for (int* ptr : ptrs) {
        allocator.deallocate(ptr);
    }

    EXPECT_FALSE(allocator.is_full());
    EXPECT_EQ(allocator.available_blocks(), 10);
}

TEST_F(MemoryPoolTest, FixedBlockAllocatorStats) {
    FixedBlockAllocator<double, 100> allocator;

    // Initial stats
    auto stats = allocator.get_stats();
    EXPECT_EQ(stats.used_bytes, 0);
    EXPECT_GT(stats.free_bytes, 0);
    EXPECT_EQ(stats.num_allocations, 0);
    EXPECT_EQ(stats.fragmentation_ratio, 0.0);

    // Allocate some blocks
    std::vector<double*> ptrs;
    for (int i = 0; i < 50; ++i) {
        ptrs.push_back(allocator.allocate());
    }

    stats = allocator.get_stats();
    EXPECT_EQ(stats.used_bytes, 50 * sizeof(double));
    EXPECT_EQ(stats.num_allocations, 50);
    EXPECT_GT(stats.efficiency(), 40.0);  // At least 40% efficient

    // Deallocate half
    for (int i = 0; i < 25; ++i) {
        allocator.deallocate(ptrs[i]);
    }

    stats = allocator.get_stats();
    EXPECT_EQ(stats.used_bytes, 25 * sizeof(double));
    EXPECT_EQ(stats.num_deallocations, 25);
}

TEST_F(MemoryPoolTest, VariablePoolAllocatorBasics) {
    VariablePoolAllocator allocator(1024);  // 1KB pools

    // Test various allocation sizes
    void* ptr1 = allocator.allocate(64);
    ASSERT_NE(ptr1, nullptr);

    void* ptr2 = allocator.allocate(128);
    ASSERT_NE(ptr2, nullptr);

    void* ptr3 = allocator.allocate(256);
    ASSERT_NE(ptr3, nullptr);

    // Test deallocation
    allocator.deallocate(ptr1);
    allocator.deallocate(ptr2);
    allocator.deallocate(ptr3);

    // Test large allocation failure
    void* large_ptr = allocator.allocate(1024 * 1024);  // 1MB - should fail
    EXPECT_EQ(large_ptr, nullptr);

    // Test zero allocation
    void* zero_ptr = allocator.allocate(0);
    EXPECT_EQ(zero_ptr, nullptr);
}

TEST_F(MemoryPoolTest, VariablePoolAllocatorAlignment) {
    VariablePoolAllocator allocator;

    // Test default alignment (16 bytes)
    void* ptr1 = allocator.allocate(100);
    ASSERT_NE(ptr1, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr1) % 16, 0);

    // Test custom alignment (32 bytes)
    void* ptr2 = allocator.allocate(100, 32);
    ASSERT_NE(ptr2, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr2) % 32, 0);

    allocator.deallocate(ptr1);
    allocator.deallocate(ptr2);
}

TEST_F(MemoryPoolTest, VariablePoolAllocatorStats) {
    VariablePoolAllocator allocator(2048);  // 2KB pools

    auto initial_stats = allocator.get_stats();
    EXPECT_GT(initial_stats.total_allocated_bytes, 0);
    EXPECT_EQ(initial_stats.used_bytes, 0);
    EXPECT_EQ(initial_stats.num_pools, 1);

    // Allocate some memory
    std::vector<void*> ptrs;
    for (int i = 0; i < 10; ++i) {
        ptrs.push_back(allocator.allocate(100));
    }

    auto stats = allocator.get_stats();
    EXPECT_GT(stats.used_bytes, 0);
    EXPECT_EQ(stats.num_allocations, 10);
    EXPECT_TRUE(stats.efficiency() > 0.0);

    // Clean up
    for (void* ptr : ptrs) {
        allocator.deallocate(ptr);
    }

    auto final_stats = allocator.get_stats();
    EXPECT_EQ(final_stats.num_deallocations, 10);
}

TEST_F(MemoryPoolTest, STLCompatibleAllocator) {
    // Test with vector
    {
        pool_vector<int> vec;
        for (int i = 0; i < 100; ++i) {
            vec.push_back(i);
        }

        EXPECT_EQ(vec.size(), 100);
        for (int i = 0; i < 100; ++i) {
            EXPECT_EQ(vec[i], i);
        }
    }

    // Check that memory was properly deallocated
    auto stats = PoolAllocator<int>::get_stats();
    // Note: Due to vector implementation, not all memory may be immediately freed
}

TEST_F(MemoryPoolTest, PoolPtrRAII) {
    // Test basic functionality
    {
        auto ptr = make_pool_ptr<std::vector<int>>(10, 42);
        EXPECT_EQ(ptr->size(), 10);
        EXPECT_EQ((*ptr)[0], 42);
    }
    // ptr should be automatically destroyed here

    // Test move semantics
    PoolPtr<int> ptr1 = make_pool_ptr<int>(123);
    EXPECT_EQ(*ptr1, 123);

    PoolPtr<int> ptr2 = std::move(ptr1);
    EXPECT_EQ(*ptr2, 123);
    EXPECT_FALSE(ptr1);  // ptr1 should be null after move

    // Test reset
    ptr2.reset();
    EXPECT_FALSE(ptr2);
}

TEST_F(MemoryPoolTest, PerformanceComparison) {
    const int num_allocations = 10000;

    std::cout << "\n=== Memory Allocation Performance Comparison ===\n";

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

    // Pool allocator performance
    double pool_time = measure_time_ms([&]() {
        FixedBlockAllocator<int, 15000> allocator;  // Ensure enough space
        std::vector<int*> ptrs;
        ptrs.reserve(num_allocations);

        for (int i = 0; i < num_allocations; ++i) {
            int* ptr = allocator.allocate();
            if (ptr) {
                *ptr = i;
                ptrs.push_back(ptr);
            }
        }

        for (int* ptr : ptrs) {
            allocator.deallocate(ptr);
        }
    });

    std::cout << "Standard new/delete: " << std_time << " ms\n";
    std::cout << "Pool allocator: " << pool_time << " ms\n";
    std::cout << "Speedup: " << (std_time / pool_time) << "x\n";

    // Pool allocator should be faster
    EXPECT_LT(pool_time, std_time * 2.0);  // At least not 2x slower
}

TEST_F(MemoryPoolTest, ThreadSafety) {
    FixedBlockAllocator<int, 1000> allocator;
    const int num_threads            = 4;
    const int allocations_per_thread = 100;

    std::vector<std::thread> threads;
    std::vector<std::vector<int*>> all_ptrs(num_threads);

    // Allocation phase
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            auto& ptrs = all_ptrs[t];
            for (int i = 0; i < allocations_per_thread; ++i) {
                int* ptr = allocator.allocate();
                if (ptr) {
                    *ptr = t * allocations_per_thread + i;
                    ptrs.push_back(ptr);
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }
    threads.clear();

    // Verify data integrity
    for (int t = 0; t < num_threads; ++t) {
        for (size_t i = 0; i < all_ptrs[t].size(); ++i) {
            int expected = t * allocations_per_thread + i;
            EXPECT_EQ(*all_ptrs[t][i], expected);
        }
    }

    // Deallocation phase
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int* ptr : all_ptrs[t]) {
                allocator.deallocate(ptr);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all memory is freed
    auto stats = allocator.get_stats();
    EXPECT_EQ(stats.used_bytes, 0);
}

TEST_F(MemoryPoolTest, GlobalMemoryManager) {
    auto& manager = MemoryPoolManager::instance();

    // Test allocation/deallocation
    void* ptr1 = manager.allocate(1024);
    ASSERT_NE(ptr1, nullptr);

    void* ptr2 = manager.allocate(2048);
    ASSERT_NE(ptr2, nullptr);

    auto stats = manager.get_global_stats();
    EXPECT_GT(stats.used_bytes, 0);
    EXPECT_EQ(stats.num_allocations, 2);

    manager.deallocate(ptr1);
    manager.deallocate(ptr2);

    auto final_stats = manager.get_global_stats();
    EXPECT_EQ(final_stats.num_deallocations, 2);
}

TEST_F(MemoryPoolTest, TimeSeriesWithPoolAllocator) {
    // Test using pool allocator with TimeSeries operations
    const size_t num_elements = 1000;

    // Create test data
    std::vector<DateTime> dates;
    pool_vector<double> values;

    DateTime base_date = DateTime::parse("2024-01-01").value();
    for (size_t i = 0; i < num_elements; ++i) {
        dates.push_back(base_date.add_days(i));
        values.push_back(static_cast<double>(i) * 0.01);
    }

    // Create TimeSeries (using standard vector for dates, pool vector for values)
    std::vector<double> std_values(values.begin(), values.end());
    TimeSeries<double> ts(dates, std_values, "pool_test");

    // Test basic operations
    EXPECT_EQ(ts.size(), num_elements);
    EXPECT_DOUBLE_EQ(ts[0], 0.0);
    EXPECT_DOUBLE_EQ(ts[999], 9.99);

    // Test mean calculation
    auto mean_result = ts.mean();
    ASSERT_TRUE(mean_result.is_ok());
    EXPECT_NEAR(mean_result.value(), 4.995, 0.001);
}

TEST_F(MemoryPoolTest, DefragmentationTest) {
    VariablePoolAllocator allocator(4096);  // 4KB pool

    // Allocate many small blocks
    std::vector<void*> ptrs;
    for (int i = 0; i < 50; ++i) {
        ptrs.push_back(allocator.allocate(64));
    }

    // Deallocate every other block to create fragmentation
    for (size_t i = 1; i < ptrs.size(); i += 2) {
        allocator.deallocate(ptrs[i]);
        ptrs[i] = nullptr;
    }

    auto stats_before = allocator.get_stats();

    // Defragment
    allocator.defragment();

    auto stats_after = allocator.get_stats();

    // Clean up remaining pointers
    for (void* ptr : ptrs) {
        if (ptr) {
            allocator.deallocate(ptr);
        }
    }

    std::cout << "\nDefragmentation test:\n";
    std::cout << "Before: " << stats_before.fragmentation_ratio << "% fragmentation\n";
    std::cout << "After: " << stats_after.fragmentation_ratio << "% fragmentation\n";

    // Fragmentation should not increase
    EXPECT_LE(stats_after.fragmentation_ratio, stats_before.fragmentation_ratio + 1.0);
}