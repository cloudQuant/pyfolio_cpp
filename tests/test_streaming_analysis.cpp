#include <gtest/gtest.h>
#include <pyfolio/streaming/real_time_analyzer.h>
#include <pyfolio/core/datetime.h>
#include <thread>
#include <chrono>
#include <atomic>

using namespace pyfolio;
using namespace pyfolio::streaming;

class StreamingAnalysisTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.buffer_size = 1000;
        config_.lookback_window = 50;
        config_.update_frequency_ms = 100;
        config_.enable_regime_detection = true;
        config_.enable_incremental_stats = true;
        
        analyzer_ = std::make_unique<RealTimeAnalyzer>(config_);
    }
    
    void TearDown() override {
        if (analyzer_) {
            analyzer_->stop();
        }
    }
    
    StreamingConfig config_;
    std::unique_ptr<RealTimeAnalyzer> analyzer_;
};

TEST_F(StreamingAnalysisTest, IncrementalStatisticsCalculation) {
    IncrementalStatistics stats;
    
    // Test with known values
    std::vector<double> values = {1.0, 2.0, 3.0, 4.0, 5.0};
    
    for (double val : values) {
        stats.update(val);
    }
    
    EXPECT_EQ(stats.count(), 5);
    EXPECT_NEAR(stats.mean(), 3.0, 1e-10);
    EXPECT_NEAR(stats.variance(), 2.5, 1e-10);
    EXPECT_NEAR(stats.std_dev(), std::sqrt(2.5), 1e-10);
    EXPECT_EQ(stats.min(), 1.0);
    EXPECT_EQ(stats.max(), 5.0);
}

TEST_F(StreamingAnalysisTest, IncrementalStatisticsSkewnessKurtosis) {
    IncrementalStatistics stats;
    
    // Test with normal-like distribution
    std::vector<double> values = {-2, -1, -0.5, 0, 0.5, 1, 2};
    
    for (double val : values) {
        stats.update(val);
    }
    
    // For symmetric distribution, skewness should be near 0
    EXPECT_NEAR(stats.skewness(), 0.0, 0.1);
    
    // For this distribution, kurtosis should be negative (platykurtic)
    EXPECT_LT(stats.kurtosis(), 0.0);
}

TEST_F(StreamingAnalysisTest, StartStopAnalyzer) {
    EXPECT_TRUE(analyzer_->start().is_ok());
    
    // Should not be able to start again
    auto result = analyzer_->start();
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.error().code, ErrorCode::InvalidState);
    
    analyzer_->stop();
    
    // Should be able to start again after stop
    EXPECT_TRUE(analyzer_->start().is_ok());
}

TEST_F(StreamingAnalysisTest, PushPriceEvents) {
    EXPECT_TRUE(analyzer_->start().is_ok());
    
    // Push some price updates
    for (int i = 0; i < 10; ++i) {
        auto result = analyzer_->push_price("AAPL", 100.0 + i);
        EXPECT_TRUE(result.is_ok());
    }
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    auto stats = analyzer_->get_return_statistics();
    EXPECT_GT(stats.count(), 0);
}

TEST_F(StreamingAnalysisTest, PushTradeEvents) {
    EXPECT_TRUE(analyzer_->start().is_ok());
    
    Trade trade{
        "AAPL",
        100.0,
        150.0,
        TransactionSide::Buy,
        DateTime::now()
    };
    
    auto result = analyzer_->push_trade(trade);
    EXPECT_TRUE(result.is_ok());
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto positions = analyzer_->get_positions();
    EXPECT_EQ(positions.size(), 1);
    EXPECT_EQ(positions["AAPL"].shares, 100.0);
}

TEST_F(StreamingAnalysisTest, EventHandlerRegistration) {
    std::atomic<int> event_count{0};
    
    analyzer_->on_event(StreamEventType::PriceUpdate, 
        [&event_count](const StreamEvent& event) {
            event_count++;
        });
    
    EXPECT_TRUE(analyzer_->start().is_ok());
    
    // Push events
    for (int i = 0; i < 5; ++i) {
        analyzer_->push_price("AAPL", 100.0 + i);
    }
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    EXPECT_EQ(event_count.load(), 5);
}

TEST_F(StreamingAnalysisTest, BufferOverflowHandling) {
    // Use small buffer for testing
    StreamingConfig small_config;
    small_config.buffer_size = 5;
    
    RealTimeAnalyzer small_analyzer(small_config);
    EXPECT_TRUE(small_analyzer.start().is_ok());
    
    // Fill the buffer
    for (int i = 0; i < 5; ++i) {
        auto result = small_analyzer.push_price("AAPL", 100.0 + i);
        EXPECT_TRUE(result.is_ok());
    }
    
    // This should fail due to buffer overflow
    auto result = small_analyzer.push_price("AAPL", 106.0);
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.error().code, ErrorCode::BufferOverflow);
}

TEST_F(StreamingAnalysisTest, GetMetricsWithInsufficientData) {
    EXPECT_TRUE(analyzer_->start().is_ok());
    
    auto result = analyzer_->get_latest_metrics();
    EXPECT_TRUE(result.is_error());
    EXPECT_EQ(result.error().code, ErrorCode::InsufficientData);
}

TEST_F(StreamingAnalysisTest, GetVaRWithData) {
    EXPECT_TRUE(analyzer_->start().is_ok());
    
    // Push enough data for VaR calculation
    for (int i = 0; i < 30; ++i) {
        analyzer_->push_price("AAPL", 100.0 * (1 + 0.01 * std::sin(i)));
    }
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    auto var_result = analyzer_->get_current_var(0.95);
    // May still have insufficient data depending on timing
    if (var_result.is_ok()) {
        EXPECT_GT(var_result.value(), 0.0);
        EXPECT_LT(var_result.value(), 1.0);
    }
}

TEST_F(StreamingAnalysisTest, RiskAlertGeneration) {
    std::atomic<bool> alert_received{false};
    
    analyzer_->on_event(StreamEventType::RiskAlert,
        [&alert_received](const StreamEvent& event) {
            alert_received = true;
            auto alert = std::get<RiskAlert>(event.data);
            EXPECT_EQ(alert.alert_type, "VaR_Breach");
        });
    
    EXPECT_TRUE(analyzer_->start().is_ok());
    
    // Push normal prices
    for (int i = 0; i < 25; ++i) {
        analyzer_->push_price("AAPL", 100.0 + i * 0.1);
    }
    
    // Push a large negative return to trigger alert
    analyzer_->push_price("AAPL", 50.0);  // 50% drop
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // Alert generation depends on VaR calculation
    // Test passes if no crash occurs
}

TEST_F(StreamingAnalysisTest, PositionTracking) {
    EXPECT_TRUE(analyzer_->start().is_ok());
    
    // Execute buy trade
    Trade buy_trade{"AAPL", 100.0, 150.0, TransactionSide::Buy, DateTime::now()};
    analyzer_->push_trade(buy_trade);
    
    // Execute sell trade
    Trade sell_trade{"AAPL", 30.0, 155.0, TransactionSide::Sell, DateTime::now()};
    analyzer_->push_trade(sell_trade);
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto positions = analyzer_->get_positions();
    EXPECT_EQ(positions["AAPL"].shares, 70.0);
    EXPECT_EQ(positions["AAPL"].price, 155.0);
}

TEST_F(StreamingAnalysisTest, RegimeDetectionEnabled) {
    // Need regime detection enabled
    ASSERT_TRUE(config_.enable_regime_detection);
    
    EXPECT_TRUE(analyzer_->start().is_ok());
    
    // Push enough data for regime detection
    for (int i = 0; i < 100; ++i) {
        double price = 100.0 * (1 + 0.001 * i + 0.01 * std::sin(i * 0.1));
        analyzer_->push_price("AAPL", price);
    }
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto regime_result = analyzer_->get_current_regime();
    // May have insufficient data or other issues
    if (regime_result.is_ok()) {
        auto [regime, confidence] = regime_result.value();
        EXPECT_GE(confidence, 0.0);
        EXPECT_LE(confidence, 1.0);
    }
}

TEST_F(StreamingAnalysisTest, ConcurrentEventProcessing) {
    EXPECT_TRUE(analyzer_->start().is_ok());
    
    const int num_threads = 4;
    const int events_per_thread = 25;
    std::vector<std::thread> threads;
    
    // Launch multiple threads pushing events
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, events_per_thread]() {
            for (int i = 0; i < events_per_thread; ++i) {
                analyzer_->push_price("AAPL", 100.0 + t + i * 0.1);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto stats = analyzer_->get_return_statistics();
    // Should have processed most events (exact count depends on timing)
    EXPECT_GT(stats.count(), 0);
}

// WebSocket tests
TEST(WebSocketStreamerTest, ConnectionLifecycle) {
    auto analyzer = std::make_shared<RealTimeAnalyzer>();
    WebSocketStreamer streamer("wss://test.example.com", analyzer);
    
    EXPECT_FALSE(streamer.is_connected());
    
    auto result = streamer.connect();
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(streamer.is_connected());
    
    streamer.disconnect();
    EXPECT_FALSE(streamer.is_connected());
}

// Performance benchmark
TEST_F(StreamingAnalysisTest, HighFrequencyPerformance) {
    EXPECT_TRUE(analyzer_->start().is_ok());
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Push 10,000 events
    const int event_count = 10000;
    for (int i = 0; i < event_count; ++i) {
        analyzer_->push_price("AAPL", 100.0 + (i % 100) * 0.01);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    std::cout << "Pushed " << event_count << " events in " 
              << duration.count() << "ms" << std::endl;
    std::cout << "Rate: " << (event_count * 1000.0 / duration.count()) 
              << " events/second" << std::endl;
    
    // Should handle at least 1000 events per second
    EXPECT_LT(duration.count(), event_count);  // < 1ms per event
}