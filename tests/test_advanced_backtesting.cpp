#include <gtest/gtest.h>
#include <pyfolio/backtesting/advanced_backtester.h>
#include <pyfolio/backtesting/strategies.h>
#include <vector>
#include <random>

using namespace pyfolio;
using namespace pyfolio::backtesting;
using namespace pyfolio::backtesting::strategies;

class AdvancedBacktestingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Generate test data
        std::vector<DateTime> dates;
        std::vector<Price> prices;
        
        // Create 100 days of test data
        for (int i = 0; i < 100; ++i) {
            dates.emplace_back(2023, 1, 1 + i);
            prices.push_back(100.0 + i * 0.1); // Slight upward trend
        }
        
        auto ts_result = TimeSeries<Price>::create(dates, prices);
        ASSERT_TRUE(ts_result.is_ok());
        test_prices_ = ts_result.value();
        
        // Create volume data
        std::vector<double> volumes(100, 1000000.0); // 1M shares daily
        auto vol_result = TimeSeries<double>::create(dates, volumes);
        ASSERT_TRUE(vol_result.is_ok());
        test_volumes_ = vol_result.value();
        
        // Create volatility data
        std::vector<double> volatilities(100, 0.02); // 2% daily vol
        auto volat_result = TimeSeries<double>::create(dates, volatilities);
        ASSERT_TRUE(volat_result.is_ok());
        test_volatilities_ = volat_result.value();
        
        // Setup basic config
        config_.start_date = DateTime(2023, 1, 1);
        config_.end_date = DateTime(2023, 4, 10);
        config_.initial_capital = 100000.0;
    }
    
    TimeSeries<Price> test_prices_;
    TimeSeries<double> test_volumes_;
    TimeSeries<double> test_volatilities_;
    BacktestConfig config_;
};

// Test commission structure calculations
TEST_F(AdvancedBacktestingTest, CommissionStructureCalculation) {
    CommissionStructure comm;
    comm.type = CommissionType::Percentage;
    comm.rate = 0.001; // 0.1%
    comm.minimum = 5.0;
    comm.maximum = 50.0;
    
    // Test percentage commission
    EXPECT_NEAR(comm.calculate_commission(10000.0, 100), 10.0, 1e-6);
    
    // Test minimum commission
    EXPECT_NEAR(comm.calculate_commission(1000.0, 10), 5.0, 1e-6);
    
    // Test maximum commission
    EXPECT_NEAR(comm.calculate_commission(100000.0, 1000), 50.0, 1e-6);
    
    // Test per-share commission
    comm.type = CommissionType::PerShare;
    comm.rate = 0.01; // 1 cent per share
    EXPECT_NEAR(comm.calculate_commission(10000.0, 100), 5.0, 1e-6); // Min applies
    EXPECT_NEAR(comm.calculate_commission(10000.0, 1000), 10.0, 1e-6);
    
    // Test fixed commission
    comm.type = CommissionType::Fixed;
    comm.rate = 9.99;
    EXPECT_NEAR(comm.calculate_commission(10000.0, 100), 9.99, 1e-6);
}

// Test market impact models
TEST_F(AdvancedBacktestingTest, MarketImpactModels) {
    MarketImpactConfig impact;
    
    // Test linear impact
    impact.model = MarketImpactModel::Linear;
    impact.impact_coefficient = 0.1;
    double linear_impact = impact.calculate_impact(1000.0, 10000.0, 0.02); // 10% participation
    EXPECT_GT(std::abs(linear_impact), 0.0);
    
    // Test square-root impact
    impact.model = MarketImpactModel::SquareRoot;
    double sqrt_impact = impact.calculate_impact(1000.0, 10000.0, 0.02);
    EXPECT_GT(std::abs(sqrt_impact), 0.0);
    EXPECT_LT(std::abs(sqrt_impact), std::abs(linear_impact)); // Should be less than linear
    
    // Test no impact
    impact.model = MarketImpactModel::None;
    double no_impact = impact.calculate_impact(1000.0, 10000.0, 0.02);
    EXPECT_EQ(no_impact, 0.0);
}

// Test slippage calculation
TEST_F(AdvancedBacktestingTest, SlippageCalculation) {
    SlippageConfig slippage;
    slippage.bid_ask_spread = 0.001;
    slippage.volatility_multiplier = 1.0;
    slippage.enable_random_slippage = false; // Disable for deterministic test
    
    std::mt19937 rng(42);
    double slip = slippage.calculate_slippage(1000.0, 0.02, rng);
    
    EXPECT_GE(slip, slippage.bid_ask_spread * 0.5);
    EXPECT_TRUE(std::isfinite(slip));
}

// Test liquidity constraints
TEST_F(AdvancedBacktestingTest, LiquidityConstraints) {
    LiquidityConstraints liquidity;
    liquidity.max_participation_rate = 0.1; // 10%
    liquidity.min_trade_size = 100.0;
    liquidity.max_trade_size = 100000.0;
    
    // Test feasible trade
    EXPECT_TRUE(liquidity.is_trade_feasible(5000.0, 100000.0)); // 5% participation
    
    // Test too large trade
    EXPECT_FALSE(liquidity.is_trade_feasible(20000.0, 100000.0)); // 20% participation
    
    // Test trade splitting
    auto chunks = liquidity.split_trade(25000.0, 100000.0);
    EXPECT_GT(chunks.size(), 1); // Should split large trade
    
    double total = 0.0;
    for (double chunk : chunks) {
        total += chunk;
        EXPECT_LE(std::abs(chunk), liquidity.max_participation_rate * 100000.0);
    }
    EXPECT_NEAR(total, 25000.0, 1e-6); // Total should equal original
}

// Test backtester initialization
TEST_F(AdvancedBacktestingTest, BacktesterInitialization) {
    AdvancedBacktester backtester(config_);
    
    // Test loading price data
    auto result = backtester.load_price_data("TEST", test_prices_);
    EXPECT_TRUE(result.is_ok());
    
    // Test loading volume data
    auto vol_result = backtester.load_volume_data("TEST", test_volumes_);
    EXPECT_TRUE(vol_result.is_ok());
    
    // Test loading volatility data
    auto volat_result = backtester.load_volatility_data("TEST", test_volatilities_);
    EXPECT_TRUE(volat_result.is_ok());
}

// Test strategy interface
class TestStrategy : public TradingStrategy {
public:
    std::unordered_map<std::string, double> generate_signals(
        const DateTime& timestamp,
        const std::unordered_map<std::string, Price>& prices,
        const PortfolioState& portfolio) override {
        
        return {{"TEST", 1.0}}; // Always 100% weight to TEST
    }
    
    std::string get_name() const override {
        return "TestStrategy";
    }
};

TEST_F(AdvancedBacktestingTest, StrategyInterface) {
    TestStrategy strategy;
    
    EXPECT_EQ(strategy.get_name(), "TestStrategy");
    
    std::unordered_map<std::string, Price> prices = {{"TEST", 100.0}};
    PortfolioState portfolio;
    
    auto signals = strategy.generate_signals(DateTime(2023, 1, 1), prices, portfolio);
    EXPECT_EQ(signals.size(), 1);
    EXPECT_EQ(signals["TEST"], 1.0);
}

// Test buy and hold strategy
TEST_F(AdvancedBacktestingTest, BuyAndHoldStrategy) {
    std::vector<std::string> symbols = {"TEST"};
    BuyAndHoldStrategy strategy(symbols);
    
    std::unordered_map<std::string, Price> prices = {{"TEST", 100.0}};
    PortfolioState portfolio;
    
    // First call should initialize positions
    auto signals1 = strategy.generate_signals(DateTime(2023, 1, 1), prices, portfolio);
    EXPECT_EQ(signals1.size(), 1);
    EXPECT_EQ(signals1["TEST"], 1.0);
    
    // Subsequent calls should maintain positions
    portfolio.positions["TEST"] = Position{100, 100.0, std::chrono::system_clock::now()};
    auto signals2 = strategy.generate_signals(DateTime(2023, 1, 2), prices, portfolio);
    EXPECT_EQ(signals2.size(), 1);
}

// Test mean reversion strategy
TEST_F(AdvancedBacktestingTest, MeanReversionStrategy) {
    std::vector<std::string> symbols = {"TEST"};
    MeanReversionStrategy strategy(symbols, 5, 0.02);
    
    std::unordered_map<std::string, Price> prices = {{"TEST", 100.0}};
    PortfolioState portfolio;
    
    // Generate signals multiple times to build history
    for (int i = 0; i < 10; ++i) {
        prices["TEST"] = 100.0 + i; // Trending up
        auto signals = strategy.generate_signals(DateTime(2023, 1, 1 + i), prices, portfolio);
        EXPECT_FALSE(signals.empty());
    }
}

// Test momentum strategy
TEST_F(AdvancedBacktestingTest, MomentumStrategy) {
    std::vector<std::string> symbols = {"TEST1", "TEST2"};
    MomentumStrategy strategy(symbols, 5, 1);
    
    std::unordered_map<std::string, Price> prices = {
        {"TEST1", 100.0},
        {"TEST2", 200.0}
    };
    PortfolioState portfolio;
    
    // Generate signals multiple times
    for (int i = 0; i < 10; ++i) {
        prices["TEST1"] = 100.0 + i * 0.5; // Slower growth
        prices["TEST2"] = 200.0 + i * 2.0; // Faster growth
        auto signals = strategy.generate_signals(DateTime(2023, 1, 1 + i), prices, portfolio);
        EXPECT_FALSE(signals.empty());
    }
}

// Test equal weight strategy
TEST_F(AdvancedBacktestingTest, EqualWeightStrategy) {
    std::vector<std::string> symbols = {"TEST1", "TEST2"};
    EqualWeightStrategy strategy(symbols, 5); // Rebalance every 5 days
    
    std::unordered_map<std::string, Price> prices = {
        {"TEST1", 100.0},
        {"TEST2", 200.0}
    };
    PortfolioState portfolio;
    
    // First call should rebalance
    auto signals1 = strategy.generate_signals(DateTime(2023, 1, 1), prices, portfolio);
    EXPECT_EQ(signals1.size(), 2);
    EXPECT_NEAR(signals1["TEST1"], 0.5, 1e-6);
    EXPECT_NEAR(signals1["TEST2"], 0.5, 1e-6);
    
    // Calls within rebalance period should maintain weights
    portfolio.positions["TEST1"] = Position{50, 100.0, std::chrono::system_clock::now()};
    portfolio.positions["TEST2"] = Position{25, 200.0, std::chrono::system_clock::now()};
    
    for (int i = 1; i < 5; ++i) {
        auto signals = strategy.generate_signals(DateTime(2023, 1, 1 + i), prices, portfolio);
        // Should maintain current weights during rebalance period
    }
}

// Test portfolio state operations
TEST_F(AdvancedBacktestingTest, PortfolioState) {
    PortfolioState portfolio;
    portfolio.cash = 50000.0;
    portfolio.positions["TEST1"] = Position{100, 100.0, std::chrono::system_clock::now()};
    portfolio.positions["TEST2"] = Position{50, 200.0, std::chrono::system_clock::now()};
    
    std::unordered_map<std::string, Price> prices = {
        {"TEST1", 110.0}, // Up 10%
        {"TEST2", 180.0}  // Down 10%
    };
    
    portfolio.update_value(prices);
    
    // Total value = cash + positions
    // = 50000 + (100 * 110) + (50 * 180)
    // = 50000 + 11000 + 9000 = 70000
    EXPECT_NEAR(portfolio.total_value, 70000.0, 1e-6);
    
    // Test weights
    auto weights = portfolio.get_weights();
    EXPECT_NEAR(weights["TEST1"], 11000.0 / 70000.0, 1e-6);
    EXPECT_NEAR(weights["TEST2"], 9000.0 / 70000.0, 1e-6);
}

// Test execution trade calculation
TEST_F(AdvancedBacktestingTest, ExecutedTradeCalculation) {
    ExecutedTrade trade;
    trade.quantity = 100;
    trade.execution_price = 105.0;
    trade.market_price = 100.0;
    
    // Implementation shortfall = (execution_price - market_price) * quantity
    double shortfall = trade.implementation_shortfall();
    EXPECT_NEAR(shortfall, 500.0, 1e-6); // (105 - 100) * 100
}

// Test backtest results structure
TEST_F(AdvancedBacktestingTest, BacktestResults) {
    BacktestResults results;
    results.initial_capital = 100000.0;
    results.final_value = 120000.0;
    results.total_commission = 500.0;
    results.total_market_impact = 300.0;
    results.total_slippage = 200.0;
    results.total_transaction_costs = 1000.0;
    
    // Test implementation shortfall calculation
    ExecutedTrade trade1;
    trade1.quantity = 100;
    trade1.execution_price = 105.0;
    trade1.market_price = 100.0;
    
    ExecutedTrade trade2;
    trade2.quantity = -50;
    trade2.execution_price = 95.0;
    trade2.market_price = 100.0;
    
    results.trade_history = {trade1, trade2};
    
    double total_shortfall = results.calculate_implementation_shortfall();
    // trade1: (105-100)*100 = 500
    // trade2: (95-100)*(-50) = 250
    // total: 750
    EXPECT_NEAR(total_shortfall, 750.0, 1e-6);
    
    // Test report generation
    std::string report = results.generate_report();
    EXPECT_FALSE(report.empty());
    EXPECT_NE(report.find("Backtest Results"), std::string::npos);
}

// Integration test with simple backtest
TEST_F(AdvancedBacktestingTest, SimpleBacktestIntegration) {
    AdvancedBacktester backtester(config_);
    
    // Load test data
    backtester.load_price_data("TEST", test_prices_);
    backtester.load_volume_data("TEST", test_volumes_);
    backtester.load_volatility_data("TEST", test_volatilities_);
    
    // Set simple strategy
    auto strategy = std::make_unique<TestStrategy>();
    backtester.set_strategy(std::move(strategy));
    
    // Run backtest
    auto result = backtester.run_backtest();
    
    // Should succeed with our simple test data
    EXPECT_TRUE(result.is_ok());
    
    if (result.is_ok()) {
        const auto& backtest_result = result.value();
        
        // Basic sanity checks
        EXPECT_EQ(backtest_result.initial_capital, config_.initial_capital);
        EXPECT_GT(backtest_result.final_value, 0);
        EXPECT_GE(backtest_result.total_trades, 0);
        EXPECT_GE(backtest_result.total_commission, 0);
    }
}

// Test error handling
TEST_F(AdvancedBacktestingTest, ErrorHandling) {
    AdvancedBacktester backtester(config_);
    
    // Test running backtest without strategy
    auto result1 = backtester.run_backtest();
    EXPECT_TRUE(result1.is_error());
    
    // Test running backtest without data
    auto strategy = std::make_unique<TestStrategy>();
    backtester.set_strategy(std::move(strategy));
    auto result2 = backtester.run_backtest();
    EXPECT_TRUE(result2.is_error());
    
    // Test loading empty price data
    auto empty_ts = TimeSeries<Price>::create({}, {});
    EXPECT_TRUE(empty_ts.is_error()); // Should fail to create empty series
}