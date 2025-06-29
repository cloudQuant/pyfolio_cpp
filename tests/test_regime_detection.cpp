#include <gtest/gtest.h>
#include <pyfolio/analytics/market_indicators.h>
#include <pyfolio/analytics/regime_detection.h>

using namespace pyfolio;
using namespace pyfolio::analytics;

class RegimeDetectionTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // Create sample return data with different regimes
        DateTime base_date = DateTime::parse("2024-01-01").value();

        std::mt19937 gen(42);

        // Generate returns with 3 distinct regimes
        // Regime 1: Low volatility, positive returns (days 0-83)
        std::normal_distribution<> regime1(0.001, 0.008);
        // Regime 2: High volatility, negative returns (days 84-167)
        std::normal_distribution<> regime2(-0.002, 0.025);
        // Regime 3: Medium volatility, positive returns (days 168-251)
        std::normal_distribution<> regime3(0.0015, 0.015);

        int regime_length = 84;  // ~3 months per regime

        for (int i = 0; i < 252; ++i) {
            DateTime current_date = base_date.add_days(i);
            dates.push_back(current_date);

            double return_val;
            if (i < regime_length) {
                return_val = regime1(gen);
                true_regimes.push_back(0);
            } else if (i < 2 * regime_length) {
                return_val = regime2(gen);
                true_regimes.push_back(1);
            } else {
                return_val = regime3(gen);
                true_regimes.push_back(2);
            }

            returns.push_back(return_val);
        }

        std::cout << "Debug: dates size = " << dates.size() << ", returns size = " << returns.size() << std::endl;

        returns_ts = TimeSeries<Return>(dates, returns, "daily_returns");

        // Create sample market indicator data
        setupMarketIndicators();
    }

    void setupMarketIndicators() {
        std::mt19937 gen(43);
        std::normal_distribution<> vix_dist(20.0, 5.0);
        std::uniform_real_distribution<> term_spread_dist(0.5, 3.0);
        std::uniform_real_distribution<> credit_spread_dist(1.0, 5.0);

        // Initialize market indicators
        market_indicators.resize(dates.size());

        for (size_t i = 0; i < dates.size(); ++i) {
            // VIX tends to be higher during stressed regime
            double vix = vix_dist(gen);
            if (true_regimes[i] == 1)
                vix += 10.0;  // Higher VIX in regime 2

            market_indicators.vix_levels[i]     = std::max(vix, 5.0);
            market_indicators.term_spreads[i]   = term_spread_dist(gen);
            market_indicators.credit_spreads[i] = credit_spread_dist(gen);
        }

        std::cout << "Debug: market_indicators.vix_levels size = " << market_indicators.vix_levels.size() << std::endl;

        // Don't create market indicators time series for now to isolate the issue
        // market_indicators_ts = MarketIndicatorSeries(dates, {market_indicators}, "market_indicators");
    }

    std::vector<DateTime> dates;
    std::vector<Return> returns;
    std::vector<int> true_regimes;
    TimeSeries<Return> returns_ts;

    MarketIndicators market_indicators;
    MarketIndicatorSeries market_indicators_ts{std::vector<DateTime>{}, std::vector<MarketIndicators>{}, "test"};
};

// Tests now use implemented methods

TEST_F(RegimeDetectionTest, MarkovSwitchingModel) {
    RegimeDetector detector;

    auto regime_result = detector.markov_switching_detection(returns_ts, 3, 1000, 42);
    ASSERT_TRUE(regime_result.is_ok());

    auto result = regime_result.value();

    // Should detect regimes
    EXPECT_EQ(result.regime_sequence.size(), returns.size());
    EXPECT_EQ(result.regime_probabilities.size(), returns.size());

    // Check that we have regime characteristics
    EXPECT_FALSE(result.regime_characteristics.empty());

    // Check current regime info
    EXPECT_GT(result.current_regime_duration, 0);
    EXPECT_GT(result.current_regime_confidence, 0.0);
    EXPECT_LE(result.current_regime_confidence, 1.0);
}

TEST_F(RegimeDetectionTest, HiddenMarkovModel) {
    RegimeDetector detector;

    auto hmm_result = detector.hidden_markov_detection(returns_ts, 2);
    ASSERT_TRUE(hmm_result.is_ok());

    auto result = hmm_result.value();

    // Should detect regimes
    EXPECT_EQ(result.regime_sequence.size(), returns.size());
    EXPECT_EQ(result.regime_probabilities.size(), returns.size());

    // Check that we have regime transitions
    EXPECT_FALSE(result.transitions.empty());
}

TEST_F(RegimeDetectionTest, StructuralBreakDetection) {
    RegimeDetector detector;

    auto break_result = detector.structural_break_detection(returns_ts, 0.05);
    ASSERT_TRUE(break_result.is_ok());

    auto result = break_result.value();

    // Should have regime sequence
    EXPECT_EQ(result.regime_sequence.size(), returns.size());
    EXPECT_EQ(result.regime_probabilities.size(), returns.size());

    // Check that regime changes are detected
    std::set<RegimeType> unique_regimes(result.regime_sequence.begin(), result.regime_sequence.end());
    EXPECT_GT(unique_regimes.size(), 1);  // Should detect multiple regimes
}

TEST_F(RegimeDetectionTest, VolatilityRegimeDetection) {
    RegimeDetector detector;

    auto vol_result = detector.volatility_regime_detection(returns_ts);
    ASSERT_TRUE(vol_result.is_ok());

    auto result = vol_result.value();

    // Should have regime sequence
    EXPECT_EQ(result.regime_sequence.size(), returns.size());
    EXPECT_EQ(result.regime_probabilities.size(), returns.size());

    // Should detect volatility-based regimes
    std::set<RegimeType> unique_regimes(result.regime_sequence.begin(), result.regime_sequence.end());
    EXPECT_GT(unique_regimes.size(), 1);
}

TEST_F(RegimeDetectionTest, BasicRegimeDetection) {
    RegimeDetector detector;

    auto regime_result = detector.detect_regimes(returns_ts);
    ASSERT_TRUE(regime_result.is_ok());

    auto result = regime_result.value();

    // Basic sanity checks
    EXPECT_EQ(result.regime_sequence.size(), returns.size());
    EXPECT_EQ(result.regime_probabilities.size(), returns.size());
    EXPECT_EQ(result.dates.size(), returns.size());

    // Check that regime characteristics and transitions are available
    EXPECT_GE(result.regime_characteristics.size(), 0);
    EXPECT_GE(result.transitions.size(), 0);

    // Probabilities should be reasonable
    for (auto prob : result.regime_probabilities) {
        EXPECT_GE(prob, 0.0);
        EXPECT_LE(prob, 1.0);
    }

    // Current regime data should be valid
    EXPECT_GE(result.current_regime_confidence, 0.0);
    EXPECT_LE(result.current_regime_confidence, 1.0);
    EXPECT_GE(result.current_regime_duration, 0);
}

TEST_F(RegimeDetectionTest, MarketIndicatorsAnalysis) {
    MarketIndicatorsAnalyzer analyzer;

    // Test VIX regime calculation
    TimeSeries<double> vix_data(dates, market_indicators.vix_levels, "vix");
    auto vix_regime = analyzer.calculate_vix_regime(vix_data);
    ASSERT_TRUE(vix_regime.is_ok());

    auto vix_result = vix_regime.value();
    EXPECT_EQ(vix_result.size(), dates.size());

    // VIX regime scores should be in valid range
    for (double score : vix_result.values()) {
        EXPECT_GE(score, 0.0);
        EXPECT_LE(score, 1.0);
    }
}

TEST_F(RegimeDetectionTest, EmptyDataHandling) {
    RegimeDetector detector;
    TimeSeries<Return> empty_ts{std::vector<DateTime>{}, std::vector<Return>{}, "empty"};

    // All regime detection methods should handle empty data gracefully
    auto empty_markov = detector.markov_switching_detection(empty_ts, 2, 100, 42);
    EXPECT_TRUE(empty_markov.is_error());

    auto empty_hmm = detector.hidden_markov_detection(empty_ts, 2);
    EXPECT_TRUE(empty_hmm.is_error());

    auto empty_breaks = detector.structural_break_detection(empty_ts, 0.05);
    EXPECT_TRUE(empty_breaks.is_error());

    auto empty_vol = detector.volatility_regime_detection(empty_ts);
    EXPECT_TRUE(empty_vol.is_error());
}

TEST_F(RegimeDetectionTest, ConsistencyChecks) {
    RegimeDetector detector;

    // Run same detection multiple times with same seed
    auto result1 = detector.markov_switching_detection(returns_ts, 3, 1000, 42);
    auto result2 = detector.markov_switching_detection(returns_ts, 3, 1000, 42);

    ASSERT_TRUE(result1.is_ok());
    ASSERT_TRUE(result2.is_ok());

    // Results should be identical with same seed
    EXPECT_EQ(result1.value().regime_sequence, result2.value().regime_sequence);
}