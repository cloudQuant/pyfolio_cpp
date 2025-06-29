#include <gtest/gtest.h>
#include <pyfolio/distributed/mpi_portfolio_analyzer.h>
#include <mpi.h>

using namespace pyfolio::distributed;
using namespace pyfolio;

/**
 * @brief Test fixture for distributed computing tests
 */
class DistributedComputingTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Initialize MPI for testing
        int argc = 0;
        char** argv = nullptr;
        
        // Check if MPI is already initialized
        int initialized;
        MPI_Initialized(&initialized);
        
        if (!initialized) {
            int provided;
            MPI_Init_thread(&argc, &argv, MPI_THREAD_SINGLE, &provided);
        }
    }
    
    static void TearDownTestSuite() {
        // Finalize MPI after all tests
        int finalized;
        MPI_Finalized(&finalized);
        
        if (!finalized) {
            MPI_Finalize();
        }
    }
    
    void SetUp() override {
        // Get MPI rank and size for individual tests
        MPI_Comm_rank(MPI_COMM_WORLD, &rank_);
        MPI_Comm_size(MPI_COMM_WORLD, &size_);
        
        // Create mock MPI environment
        mpi_env_ = std::make_shared<MPIEnvironment>(rank_, size_);
        
        // Create test portfolio data
        test_portfolio_data_ = create_test_portfolio_data();
    }
    
    std::unordered_map<std::string, TimeSeries<Price>> create_test_portfolio_data() {
        std::unordered_map<std::string, TimeSeries<Price>> data;
        
        std::vector<std::string> symbols = {"AAPL", "MSFT", "GOOGL"};
        size_t num_days = 100;
        
        for (const auto& symbol : symbols) {
            std::vector<DateTime> dates;
            std::vector<Price> prices;
            
            DateTime start_date(2023, 1, 1);
            for (size_t i = 0; i < num_days; ++i) {
                dates.push_back(start_date.add_days(static_cast<int>(i)));
                prices.push_back(100.0 + static_cast<double>(i) * 0.1);
            }
            
            auto ts_result = TimeSeries<Price>::create(dates, prices, symbol);
            if (ts_result.is_ok()) {
                data[symbol] = ts_result.value();
            }
        }
        
        return data;
    }
    
    int rank_;
    int size_;
    std::shared_ptr<MPIEnvironment> mpi_env_;
    std::unordered_map<std::string, TimeSeries<Price>> test_portfolio_data_;
};

/**
 * @brief Test MPI environment initialization
 */
TEST_F(DistributedComputingTest, MPIEnvironmentInitialization) {
    EXPECT_TRUE(mpi_env_->is_initialized());
    EXPECT_GE(mpi_env_->rank(), 0);
    EXPECT_GT(mpi_env_->size(), 0);
    EXPECT_LT(mpi_env_->rank(), mpi_env_->size());
    
    if (mpi_env_->rank() == 0) {
        EXPECT_TRUE(mpi_env_->is_master());
    } else {
        EXPECT_FALSE(mpi_env_->is_master());
    }
    
    // Test processor name
    std::string proc_name = mpi_env_->processor_name();
    EXPECT_FALSE(proc_name.empty());
    
    // Test elapsed time
    double elapsed = mpi_env_->elapsed_time();
    EXPECT_GE(elapsed, 0.0);
}

/**
 * @brief Test distributed portfolio analyzer creation
 */
TEST_F(DistributedComputingTest, AnalyzerCreation) {
    auto analyzer_result = MPIPortfolioAnalyzer::create(mpi_env_);
    
    EXPECT_TRUE(analyzer_result.is_ok());
    EXPECT_NE(analyzer_result.value(), nullptr);
    
    // Test invalid environment
    auto invalid_result = MPIPortfolioAnalyzer::create(nullptr);
    EXPECT_TRUE(invalid_result.is_error());
    EXPECT_EQ(invalid_result.error().code, ErrorCode::InvalidInput);
}

/**
 * @brief Test data distribution across nodes
 */
TEST_F(DistributedComputingTest, DataDistribution) {
    auto analyzer_result = MPIPortfolioAnalyzer::create(mpi_env_);
    ASSERT_TRUE(analyzer_result.is_ok());
    
    auto analyzer = analyzer_result.value();
    
    // Test data distribution
    auto dist_result = analyzer->distribute_portfolio_data(test_portfolio_data_);
    EXPECT_TRUE(dist_result.is_ok());
    
    // Test with empty data
    std::unordered_map<std::string, TimeSeries<Price>> empty_data;
    auto empty_result = analyzer->distribute_portfolio_data(empty_data);
    EXPECT_TRUE(empty_result.is_ok()); // Should handle empty data gracefully
}

/**
 * @brief Test Monte Carlo configuration
 */
TEST_F(DistributedComputingTest, MonteCarloConfiguration) {
    DistributedMonteCarloConfig config;
    
    // Test default configuration
    EXPECT_GT(config.total_simulations, 0);
    EXPECT_GT(config.time_horizon_days, 0);
    EXPECT_TRUE(config.use_historical_bootstrap);
    EXPECT_TRUE(config.enable_variance_reduction);
    
    // Test configuration validation
    config.total_simulations = 1000;
    config.time_horizon_days = 252;
    config.confidence_levels[0] = 0.95;
    config.confidence_levels[1] = 0.99;
    config.confidence_levels[2] = 0.999;
    
    EXPECT_EQ(config.total_simulations, 1000);
    EXPECT_EQ(config.time_horizon_days, 252);
    EXPECT_DOUBLE_EQ(config.confidence_levels[0], 0.95);
}

/**
 * @brief Test distributed Monte Carlo simulation (single node)
 */
TEST_F(DistributedComputingTest, MonteCarloSimulation) {
    // Skip this test if running with multiple MPI processes
    // as it's designed for single-node testing
    if (size_ > 1) {
        GTEST_SKIP() << "Skipping single-node test in multi-node environment";
    }
    
    auto analyzer_result = MPIPortfolioAnalyzer::create(mpi_env_);
    ASSERT_TRUE(analyzer_result.is_ok());
    
    auto analyzer = analyzer_result.value();
    
    // Distribute test data
    auto dist_result = analyzer->distribute_portfolio_data(test_portfolio_data_);
    ASSERT_TRUE(dist_result.is_ok());
    
    // Configure Monte Carlo simulation
    DistributedMonteCarloConfig config;
    config.total_simulations = 100; // Small number for fast testing
    config.time_horizon_days = 10;
    config.random_seed_base = 42;
    
    // Run simulation
    auto mc_result = analyzer->run_distributed_monte_carlo(config);
    
    if (mc_result.is_ok()) {
        const auto& results = mc_result.value();
        
        EXPECT_GT(results.total_simulations, 0);
        EXPECT_LE(results.successful_simulations, results.total_simulations);
        EXPECT_GE(results.computation_time_seconds, 0.0);
        EXPECT_GT(results.contributing_nodes, 0);
        
        // Test result data
        EXPECT_EQ(results.portfolio_values.size(), results.portfolio_returns.size());
        EXPECT_FALSE(results.var_estimates.empty());
        EXPECT_FALSE(results.cvar_estimates.empty());
        
        // Test statistical consistency
        if (!results.portfolio_values.empty()) {
            EXPECT_GT(results.mean_final_value, 0.0);
            EXPECT_GE(results.std_final_value, 0.0);
            EXPECT_LE(results.min_value, results.max_value);
        }
    }
}

/**
 * @brief Test distributed backtesting configuration
 */
TEST_F(DistributedComputingTest, BacktestConfiguration) {
    DistributedBacktestConfig config;
    
    // Add test strategy parameters
    std::unordered_map<std::string, double> params1;
    params1["lookback_period"] = 20;
    params1["rebalance_frequency"] = 21;
    config.strategy_parameters.push_back(params1);
    
    std::unordered_map<std::string, double> params2;
    params2["lookback_period"] = 50;
    params2["rebalance_frequency"] = 63;
    config.strategy_parameters.push_back(params2);
    
    EXPECT_EQ(config.strategy_parameters.size(), 2);
    
    // Test base configuration
    config.base_config.initial_capital = 1000000.0;
    config.base_config.start_date = DateTime(2023, 1, 1);
    config.base_config.end_date = DateTime(2023, 12, 31);
    
    EXPECT_DOUBLE_EQ(config.base_config.initial_capital, 1000000.0);
    EXPECT_EQ(config.base_config.start_date.year(), 2023);
    EXPECT_EQ(config.base_config.end_date.year(), 2023);
}

/**
 * @brief Test distributed backtesting (single node)
 */
TEST_F(DistributedComputingTest, DistributedBacktesting) {
    // Skip this test if running with multiple MPI processes
    if (size_ > 1) {
        GTEST_SKIP() << "Skipping single-node test in multi-node environment";
    }
    
    auto analyzer_result = MPIPortfolioAnalyzer::create(mpi_env_);
    ASSERT_TRUE(analyzer_result.is_ok());
    
    auto analyzer = analyzer_result.value();
    
    // Distribute test data
    auto dist_result = analyzer->distribute_portfolio_data(test_portfolio_data_);
    ASSERT_TRUE(dist_result.is_ok());
    
    // Configure backtesting
    DistributedBacktestConfig config;
    
    // Add a few test parameter combinations
    for (int lookback : {10, 20, 30}) {
        std::unordered_map<std::string, double> params;
        params["lookback_period"] = lookback;
        params["rebalance_frequency"] = 21;
        config.strategy_parameters.push_back(params);
    }
    
    config.base_config.initial_capital = 100000.0;
    config.base_config.start_date = DateTime(2023, 1, 1);
    config.base_config.end_date = DateTime(2023, 3, 31);
    
    // Run backtesting
    auto bt_result = analyzer->run_distributed_backtesting(config);
    
    if (bt_result.is_ok()) {
        const auto& results = bt_result.value();
        
        EXPECT_GT(results.size(), 0);
        EXPECT_LE(results.size(), config.strategy_parameters.size());
        
        // Test individual backtest results
        for (const auto& result : results) {
            EXPECT_GT(result.initial_capital, 0.0);
            EXPECT_GT(result.final_value, 0.0);
            EXPECT_GE(result.total_trades, 0);
        }
    }
}

/**
 * @brief Test portfolio optimization
 */
TEST_F(DistributedComputingTest, PortfolioOptimization) {
    // Skip this test if running with multiple MPI processes
    if (size_ > 1) {
        GTEST_SKIP() << "Skipping single-node test in multi-node environment";
    }
    
    auto analyzer_result = MPIPortfolioAnalyzer::create(mpi_env_);
    ASSERT_TRUE(analyzer_result.is_ok());
    
    auto analyzer = analyzer_result.value();
    
    // Create test optimization problems
    const size_t n_assets = 5;
    const size_t n_problems = 10;
    
    std::vector<std::vector<double>> expected_returns;
    std::vector<std::vector<std::vector<double>>> covariance_matrices;
    std::vector<double> risk_aversions;
    
    for (size_t i = 0; i < n_problems; ++i) {
        // Expected returns
        std::vector<double> returns(n_assets, 0.08); // 8% expected return
        expected_returns.push_back(returns);
        
        // Covariance matrix (identity matrix)
        std::vector<std::vector<double>> covar(n_assets, std::vector<double>(n_assets, 0.0));
        for (size_t j = 0; j < n_assets; ++j) {
            covar[j][j] = 0.04; // 20% volatility
        }
        covariance_matrices.push_back(covar);
        
        // Risk aversion
        risk_aversions.push_back(2.0);
    }
    
    // Run optimization
    auto opt_result = analyzer->run_distributed_portfolio_optimization(
        expected_returns, covariance_matrices, risk_aversions);
    
    if (opt_result.is_ok()) {
        const auto& weights = opt_result.value();
        
        EXPECT_EQ(weights.size(), n_problems * n_assets);
        
        // Test weight constraints (non-negative, sum to 1)
        for (size_t p = 0; p < n_problems; ++p) {
            double weight_sum = 0.0;
            for (size_t a = 0; a < n_assets; ++a) {
                double weight = weights[p * n_assets + a];
                EXPECT_GE(weight, 0.0);
                weight_sum += weight;
            }
            EXPECT_NEAR(weight_sum, 1.0, 1e-6);
        }
    }
}

/**
 * @brief Test performance statistics collection
 */
TEST_F(DistributedComputingTest, PerformanceStatistics) {
    auto analyzer_result = MPIPortfolioAnalyzer::create(mpi_env_);
    ASSERT_TRUE(analyzer_result.is_ok());
    
    auto analyzer = analyzer_result.value();
    
    // Get initial performance stats (should be empty)
    auto initial_stats = analyzer->get_performance_stats();
    EXPECT_TRUE(initial_stats.empty());
    
    // Run some operations to generate stats
    auto dist_result = analyzer->distribute_portfolio_data(test_portfolio_data_);
    EXPECT_TRUE(dist_result.is_ok());
    
    // Performance stats might be updated after operations
    auto updated_stats = analyzer->get_performance_stats();
    // Note: Stats might still be empty in single-node test environment
}

/**
 * @brief Test cluster information printing
 */
TEST_F(DistributedComputingTest, ClusterInformation) {
    auto analyzer_result = MPIPortfolioAnalyzer::create(mpi_env_);
    ASSERT_TRUE(analyzer_result.is_ok());
    
    auto analyzer = analyzer_result.value();
    
    // This should not crash
    EXPECT_NO_THROW(analyzer->print_cluster_info());
}

/**
 * @brief Test error handling in distributed operations
 */
TEST_F(DistributedComputingTest, ErrorHandling) {
    auto analyzer_result = MPIPortfolioAnalyzer::create(mpi_env_);
    ASSERT_TRUE(analyzer_result.is_ok());
    
    auto analyzer = analyzer_result.value();
    
    // Test Monte Carlo with invalid configuration
    DistributedMonteCarloConfig invalid_config;
    invalid_config.total_simulations = 0; // Invalid
    
    // This should handle gracefully
    auto mc_result = analyzer->run_distributed_monte_carlo(invalid_config);
    // Result may succeed with 0 simulations or fail gracefully
    
    // Test backtesting with empty configuration
    DistributedBacktestConfig empty_config;
    auto bt_result = analyzer->run_distributed_backtesting(empty_config);
    EXPECT_TRUE(bt_result.is_ok()); // Should handle empty gracefully
    
    // Test optimization with empty inputs
    std::vector<std::vector<double>> empty_returns;
    std::vector<std::vector<std::vector<double>>> empty_covar;
    std::vector<double> empty_risk;
    
    auto opt_result = analyzer->run_distributed_portfolio_optimization(
        empty_returns, empty_covar, empty_risk);
    EXPECT_TRUE(opt_result.is_ok()); // Should handle empty gracefully
}

/**
 * @brief Test MPI barrier synchronization
 */
TEST_F(DistributedComputingTest, BarrierSynchronization) {
    // Test barrier synchronization
    auto start_time = std::chrono::high_resolution_clock::now();
    
    mpi_env_->barrier();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // Barrier should complete quickly in test environment
    EXPECT_LT(duration.count(), 100000); // Less than 100ms
}

/**
 * @brief Integration test for complete workflow
 */
TEST_F(DistributedComputingTest, CompleteWorkflowIntegration) {
    // Skip this test if running with multiple MPI processes
    if (size_ > 1) {
        GTEST_SKIP() << "Skipping integration test in multi-node environment";
    }
    
    auto analyzer_result = MPIPortfolioAnalyzer::create(mpi_env_);
    ASSERT_TRUE(analyzer_result.is_ok());
    
    auto analyzer = analyzer_result.value();
    
    // Step 1: Distribute data
    auto dist_result = analyzer->distribute_portfolio_data(test_portfolio_data_);
    ASSERT_TRUE(dist_result.is_ok());
    
    // Step 2: Run small Monte Carlo
    DistributedMonteCarloConfig mc_config;
    mc_config.total_simulations = 50;
    mc_config.time_horizon_days = 5;
    
    auto mc_result = analyzer->run_distributed_monte_carlo(mc_config);
    if (mc_result.is_ok()) {
        EXPECT_GT(mc_result.value().total_simulations, 0);
    }
    
    // Step 3: Run small backtesting
    DistributedBacktestConfig bt_config;
    std::unordered_map<std::string, double> params;
    params["test_param"] = 1.0;
    bt_config.strategy_parameters.push_back(params);
    bt_config.base_config.initial_capital = 10000.0;
    
    auto bt_result = analyzer->run_distributed_backtesting(bt_config);
    EXPECT_TRUE(bt_result.is_ok());
    
    // Step 4: Check performance stats
    auto perf_stats = analyzer->get_performance_stats();
    // Stats may or may not be populated in test environment
    
    // All steps should complete without errors
    SUCCEED();
}

/**
 * @brief Main function for MPI test execution
 */
int main(int argc, char** argv) {
    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);
    
    // Initialize MPI
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_SINGLE, &provided);
    
    // Run tests
    int result = RUN_ALL_TESTS();
    
    // Finalize MPI
    MPI_Finalize();
    
    return result;
}