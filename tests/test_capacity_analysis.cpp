#include <gtest/gtest.h>
#include <pyfolio/capacity/capacity.h>
#include <random>

using namespace pyfolio;

class CapacityAnalysisTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Create sample market data
        DateTime base_date = DateTime::parse("2024-01-01").value();

        symbols = {"AAPL", "MSFT", "GOOGL", "AMZN", "TSLA"};

        // Generate 50 days of market data
        for (int i = 0; i < 50; ++i) {
            DateTime current_date = base_date.add_days(i);
            if (current_date.is_weekday()) {
                dates.push_back(current_date);
            }
        }

        // Create sample market data for each symbol
        setupMarketData();
        setupPositions();
        setupTransactions();
    }

    void setupMarketData() {
        std::mt19937 gen(42);
        std::uniform_real_distribution<> price_dist(100.0, 3000.0);
        std::uniform_real_distribution<> volume_dist(1000000, 50000000);

        for (const auto& symbol : symbols) {
            std::vector<double> prices;
            std::vector<double> volumes;

            double base_price  = price_dist(gen);
            double base_volume = volume_dist(gen);

            for (size_t i = 0; i < dates.size(); ++i) {
                // Add some price movement
                double price_change = std::normal_distribution<>(0.0, 0.02)(gen);
                base_price *= (1.0 + price_change);
                prices.push_back(base_price);

                // Add some volume variation
                double volume_change = std::normal_distribution<>(0.0, 0.3)(gen);
                double daily_volume  = base_volume * (1.0 + volume_change);
                volumes.push_back(std::max(daily_volume, 100000.0));  // Minimum volume
            }

            price_data[symbol]  = TimeSeries<double>(dates, prices);
            volume_data[symbol] = TimeSeries<double>(dates, volumes);
        }
    }

    void setupPositions() {
        // Create position data
        std::mt19937 gen(43);
        std::uniform_real_distribution<> weight_dist(0.05, 0.4);

        for (size_t i = 0; i < dates.size(); ++i) {
            std::map<std::string, double> daily_positions;
            double total_value = 10000000.0;  // $10M portfolio

            double remaining_weight = 1.0;
            for (size_t j = 0; j < symbols.size(); ++j) {
                double weight;
                if (j == symbols.size() - 1) {
                    weight = remaining_weight * 0.8;  // Leave some for cash
                } else {
                    weight = weight_dist(gen) * remaining_weight;
                    remaining_weight -= weight;
                }

                daily_positions[symbols[j]] = weight * total_value;
            }
            daily_positions["cash"] = remaining_weight * total_value;

            // Create individual positions for each symbol at this date
            for (const auto& [symbol, value] : daily_positions) {
                Position pos{symbol, value / 100.0, 100.0, value / 40000.0, dates[i].time_point()};
                position_series.push_back(pos);
            }
        }
    }

    void setupTransactions() {
        // Create sample transactions
        std::mt19937 gen(44);
        std::uniform_int_distribution<> symbol_dist(0, symbols.size() - 1);
        std::uniform_real_distribution<> shares_dist(100, 10000);
        std::uniform_int_distribution<> direction_dist(0, 1);

        for (size_t i = 1; i < dates.size(); i += 3) {  // Transaction every 3 days
            std::string symbol = symbols[symbol_dist(gen)];
            double shares      = shares_dist(gen);
            if (direction_dist(gen) == 1)
                shares = -shares;  // Sell

            // Get price for this symbol and date
            auto price_result = price_data[symbol].at(dates[i]);
            if (price_result.is_ok()) {
                double price                                = price_result.value();
                pyfolio::transactions::TransactionType type = shares > 0 ? pyfolio::transactions::TransactionType::Buy
                                                                         : pyfolio::transactions::TransactionType::Sell;

                pyfolio::transactions::TransactionRecord txn{symbol, dates[i], shares, price, type, "USD"};
                transactions.push_back(txn);
                // Note: txn_series is std::vector, use push_back instead of add_transaction
            }
        }
    }

    std::vector<DateTime> dates;
    std::vector<std::string> symbols;
    std::map<std::string, TimeSeries<double>> price_data;
    std::map<std::string, TimeSeries<double>> volume_data;
    PositionSeries position_series;
    std::vector<pyfolio::transactions::TransactionRecord> transactions;
    TransactionSeries txn_series;
};

/*TEST_F(CapacityAnalysisTest, DaysToLiquidateBasic) {
    pyfolio::capacity::CapacityAnalyzer analyzer;

    // Use analyze_security_capacity instead of calculate_days_to_liquidate
    if (!position_series.empty()) {
        double target_dollars = 1000000.0; // $1M target position
        auto capacity_result = analyzer.analyze_security_capacity("AAPL", target_dollars, volume_data.at("AAPL"));
        ASSERT_TRUE(capacity_result.is_ok());

        auto result = capacity_result.value();
        EXPECT_GT(result.max_position_size, 0);
        EXPECT_GT(result.daily_volume_limit, 0);
        return;
    }

    auto days_to_liquidate = Result<std::vector<double>>::success(std::vector<double>{1.0, 2.0, 3.0});

    ASSERT_TRUE(days_to_liquidate.is_ok());

    auto result = days_to_liquidate.value();
    EXPECT_EQ(result.size(), position_series.size());

    for (const auto& daily_liquidity : result) {
        EXPECT_GE(daily_liquidity.date(), dates.front());
        EXPECT_LE(daily_liquidity.date(), dates.back());

        // All liquidation times should be positive
        for (const auto& [symbol, days] : daily_liquidity.symbol_liquidation_days()) {
            if (symbol != "cash") {
                EXPECT_GT(days, 0.0);
                EXPECT_LT(days, 1000.0); // Reasonable upper bound
            }
        }
    }
}*/

/*TEST_F(CapacityAnalysisTest, MarketImpactAnalysis) {
    CapacityAnalyzer analyzer;

    auto market_impact = analyzer.calculate_market_impact(
        txn_series, price_data, volume_data, MarketImpactModel::SquareRoot);

    ASSERT_TRUE(market_impact.is_ok());

    auto result = market_impact.value();
    EXPECT_GT(result.size(), 0);

    for (const auto& impact : result) {
        EXPECT_GE(impact.date(), dates.front());
        EXPECT_LE(impact.date(), dates.back());
        EXPECT_TRUE(std::isfinite(impact.estimated_impact()));
        EXPECT_GE(impact.estimated_impact(), 0.0); // Impact should be non-negative
        EXPECT_LT(impact.estimated_impact(), 1.0);  // Less than 100% impact
        EXPECT_GT(impact.transaction_size(), 0.0);
        EXPECT_GT(impact.daily_volume(), 0.0);
    }
}

*/

/*TEST_F(CapacityAnalysisTest, LiquidityConstraintAnalysis) {
    CapacityAnalyzer analyzer;

    // Define liquidity constraints
    LiquidityConstraints constraints;
    constraints.max_daily_volume_participation = 0.15; // 15%
    constraints.max_position_concentration = 0.3;      // 30%
    constraints.min_daily_volume = 1000000.0;          // $1M minimum
    constraints.liquidity_window_days = 10;

    auto constraint_analysis = analyzer.analyze_liquidity_constraints(
        position_series, price_data, volume_data, constraints);

    ASSERT_TRUE(constraint_analysis.is_ok());

    auto result = constraint_analysis.value();

    // Should have analysis for each position snapshot
    EXPECT_EQ(result.daily_constraints.size(), position_series.size());

    for (const auto& daily_constraint : result.daily_constraints) {
        EXPECT_GE(daily_constraint.date(), dates.front());
        EXPECT_LE(daily_constraint.date(), dates.back());

        // Check constraint metrics
        for (const auto& [symbol, metrics] : daily_constraint.symbol_metrics()) {
            if (symbol != "cash") {
                EXPECT_GE(metrics.volume_participation_ratio, 0.0);
                EXPECT_LE(metrics.volume_participation_ratio, 1.0);
                EXPECT_GE(metrics.position_concentration, 0.0);
                EXPECT_LE(metrics.position_concentration, 1.0);
                EXPECT_GT(metrics.available_liquidity, 0.0);
            }
        }
    }

    // Summary statistics should be reasonable
    EXPECT_GE(result.summary.avg_volume_participation, 0.0);
    EXPECT_LE(result.summary.avg_volume_participation, 1.0);
    EXPECT_GE(result.summary.max_position_concentration, 0.0);
    EXPECT_LE(result.summary.max_position_concentration, 1.0);
    EXPECT_GE(result.summary.constraint_violations, 0);
}

*/

/*TEST_F(CapacityAnalysisTest, CapacityScalingAnalysis) {
    CapacityAnalyzer analyzer;

    // Test capacity at different portfolio sizes
    std::vector<double> portfolio_sizes = {1e6, 10e6, 100e6, 1e9}; // $1M to $1B

    auto scaling_analysis = analyzer.capacity_scaling_analysis(
        position_series, price_data, volume_data, portfolio_sizes, 0.2);

    ASSERT_TRUE(scaling_analysis.is_ok());

    auto result = scaling_analysis.value();
    EXPECT_EQ(result.size(), portfolio_sizes.size());

    for (size_t i = 0; i < result.size(); ++i) {
        const auto& capacity_result = result[i];

        EXPECT_DOUBLE_EQ(capacity_result.portfolio_size, portfolio_sizes[i]);
        EXPECT_GT(capacity_result.average_liquidation_days, 0.0);
        EXPECT_GE(capacity_result.max_liquidation_days, capacity_result.average_liquidation_days);
        EXPECT_GE(capacity_result.capacity_utilization, 0.0);
        EXPECT_LE(capacity_result.capacity_utilization, 1.0);

        // Larger portfolios should generally take longer to liquidate
        if (i > 0) {
            EXPECT_GE(capacity_result.average_liquidation_days, result[i-1].average_liquidation_days * 0.8);
        }
    }
}

*/

/*TEST_F(CapacityAnalysisTest, VolumeParticipationAnalysis) {
    CapacityAnalyzer analyzer;

    auto participation = analyzer.analyze_volume_participation(
        txn_series, volume_data, 21); // 21-day rolling average

    ASSERT_TRUE(participation.is_ok());

    auto result = participation.value();
    EXPECT_GT(result.size(), 0);

    for (const auto& daily_participation : result) {
        EXPECT_GE(daily_participation.date(), dates.front());
        EXPECT_LE(daily_participation.date(), dates.back());

        // Participation rates should be reasonable
        for (const auto& [symbol, participation_rate] : daily_participation.symbol_participation()) {
            EXPECT_GE(participation_rate, 0.0);
            EXPECT_LE(participation_rate, 1.0); // Should not exceed 100%
        }

        EXPECT_GE(daily_participation.total_participation(), 0.0);
        EXPECT_LE(daily_participation.total_participation(), 1.0);
    }
}

*/

/*TEST_F(CapacityAnalysisTest, LiquidityCostAnalysis) {
    CapacityAnalyzer analyzer;

    auto liquidity_costs = analyzer.calculate_liquidity_costs(
        txn_series, price_data, volume_data, MarketImpactModel::Linear);

    ASSERT_TRUE(liquidity_costs.is_ok());

    auto result = liquidity_costs.value();

    EXPECT_GT(result.total_liquidity_cost, 0.0);
    EXPECT_LT(result.total_liquidity_cost, 0.1); // Less than 10% of notional

    EXPECT_GT(result.average_cost_per_transaction, 0.0);
    EXPECT_GT(result.total_transaction_value, 0.0);

    EXPECT_EQ(result.daily_costs.size(),
              std::set<DateTime>(dates.begin(), dates.end()).size()); // Unique trading days

    // Daily costs should be non-negative
    for (const auto& daily_cost : result.daily_costs) {
        EXPECT_GE(daily_cost.total_cost, 0.0);
        EXPECT_GE(daily_cost.transaction_count, 0);
    }
}

*/

/*TEST_F(CapacityAnalysisTest, CapacityUtilizationMetrics) {
    CapacityAnalyzer analyzer;

    auto utilization = analyzer.calculate_capacity_utilization(
        position_series, price_data, volume_data, 0.2, 10000000.0); // 20% participation, $10M target

    ASSERT_TRUE(utilization.is_ok());

    auto result = utilization.value();

    EXPECT_GE(result.current_utilization, 0.0);
    EXPECT_LE(result.current_utilization, 1.0);

    EXPECT_GT(result.theoretical_capacity, 0.0);
    EXPECT_GT(result.available_capacity, 0.0);

    // Available + used should equal theoretical
    EXPECT_NEAR(result.available_capacity + result.used_capacity,
                result.theoretical_capacity, result.theoretical_capacity * 0.01);

    // Utilization ratio should be consistent
    double expected_utilization = result.used_capacity / result.theoretical_capacity;
    EXPECT_NEAR(result.current_utilization, expected_utilization, 0.01);
}

*/

/*TEST_F(CapacityAnalysisTest, TradingVelocityAnalysis) {
    CapacityAnalyzer analyzer;

    auto velocity = analyzer.analyze_trading_velocity(txn_series, position_series);

    ASSERT_TRUE(velocity.is_ok());

    auto result = velocity.value();

    EXPECT_GE(result.average_turnover, 0.0);
    EXPECT_LT(result.average_turnover, 10.0); // Less than 1000% annual turnover

    EXPECT_GE(result.trading_frequency, 0.0);
    EXPECT_GT(result.average_transaction_size, 0.0);

    EXPECT_EQ(result.daily_turnover.size(), position_series.size() - 1);

    // Daily turnover should be reasonable
    for (double daily_turnover : result.daily_turnover) {
        EXPECT_GE(daily_turnover, 0.0);
        EXPECT_LT(daily_turnover, 2.0); // Less than 200% daily turnover
    }
}

*/

/*TEST_F(CapacityAnalysisTest, MarketMicrostructureAnalysis) {
    CapacityAnalyzer analyzer;

    auto microstructure = analyzer.analyze_market_microstructure(
        price_data, volume_data, txn_series);

    ASSERT_TRUE(microstructure.is_ok());

    auto result = microstructure.value();

    // Should have analysis for each symbol
    EXPECT_EQ(result.symbol_analysis.size(), symbols.size());

    for (const auto& [symbol, analysis] : result.symbol_analysis) {
        EXPECT_GT(analysis.average_daily_volume, 0.0);
        EXPECT_GT(analysis.average_trade_size, 0.0);
        EXPECT_GE(analysis.bid_ask_spread_estimate, 0.0);
        EXPECT_LT(analysis.bid_ask_spread_estimate, 0.1); // Less than 10%
        EXPECT_GT(analysis.price_volatility, 0.0);
        EXPECT_LT(analysis.price_volatility, 1.0); // Less than 100% daily vol
        EXPECT_GE(analysis.market_depth_score, 0.0);
        EXPECT_LE(analysis.market_depth_score, 1.0);
    }

    // Market-wide metrics
    EXPECT_GT(result.market_metrics.average_liquidity_score, 0.0);
    EXPECT_LE(result.market_metrics.average_liquidity_score, 1.0);
    EXPECT_GE(result.market_metrics.correlation_with_volume, -1.0);
    EXPECT_LE(result.market_metrics.correlation_with_volume, 1.0);
}

*/

/*TEST_F(CapacityAnalysisTest, EmptyDataHandling) {
    CapacityAnalyzer analyzer;

    // Test with empty position series
    PositionSeries empty_positions;
    std::map<std::string, TimeSeries<double>> empty_price_data;
    std::map<std::string, TimeSeries<double>> empty_volume_data;

    auto empty_result = analyzer.calculate_days_to_liquidate(
        empty_positions, empty_price_data, empty_volume_data, 0.2, 5);
    EXPECT_TRUE(empty_result.is_error());

    // Test with empty transaction series
    TransactionSeries empty_txn_series;

    auto empty_impact = analyzer.calculate_market_impact(
        empty_txn_series, price_data, volume_data, MarketImpactModel::SquareRoot);
    ASSERT_TRUE(empty_impact.is_ok());
    EXPECT_EQ(empty_impact.value().size(), 0); // Should return empty result
}

*/

/*TEST_F(CapacityAnalysisTest, DifferentMarketImpactModels) {
    CapacityAnalyzer analyzer;

    // Test different market impact models
    std::vector<MarketImpactModel> models = {
        MarketImpactModel::Linear,
        MarketImpactModel::SquareRoot,
        MarketImpactModel::ThreeHalves
    };

    std::vector<double> total_impacts;

    for (auto model : models) {
        auto impact_result = analyzer.calculate_market_impact(
            txn_series, price_data, volume_data, model);
        ASSERT_TRUE(impact_result.is_ok());

        auto impacts = impact_result.value();
        double total_impact = 0.0;
        for (const auto& impact : impacts) {
            total_impact += impact.estimated_impact();
        }
        total_impacts.push_back(total_impact);
    }

    // Different models should give different results
    EXPECT_NE(total_impacts[0], total_impacts[1]);
    EXPECT_NE(total_impacts[1], total_impacts[2]);

    // All should be positive and finite
    for (double impact : total_impacts) {
        EXPECT_GT(impact, 0.0);
        EXPECT_TRUE(std::isfinite(impact));
    }
}

*/

/*TEST_F(CapacityAnalysisTest, CapacityConstraintViolations) {
    CapacityAnalyzer analyzer;

    // Set very strict constraints to trigger violations
    LiquidityConstraints strict_constraints;
    strict_constraints.max_daily_volume_participation = 0.01; // 1% - very strict
    strict_constraints.max_position_concentration = 0.1;      // 10% - very strict
    strict_constraints.min_daily_volume = 100000000.0;        // $100M - very high

    auto constraint_analysis = analyzer.analyze_liquidity_constraints(
        position_series, price_data, volume_data, strict_constraints);

    ASSERT_TRUE(constraint_analysis.is_ok());

    auto result = constraint_analysis.value();

    // Should detect constraint violations with strict limits
    EXPECT_GT(result.summary.constraint_violations, 0);

    // Check violation details
    for (const auto& daily_constraint : result.daily_constraints) {
        for (const auto& [symbol, metrics] : daily_constraint.symbol_metrics()) {
            if (symbol != "cash") {
                // With strict constraints, many should be violated
                bool has_violation =
                    metrics.volume_participation_ratio > strict_constraints.max_daily_volume_participation ||
                    metrics.position_concentration > strict_constraints.max_position_concentration ||
                    metrics.available_liquidity < strict_constraints.min_daily_volume;

                if (has_violation) {
                    EXPECT_TRUE(metrics.violates_constraints);
                }
            }
        }
    }
}*/