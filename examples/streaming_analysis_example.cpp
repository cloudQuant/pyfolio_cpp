#include <pyfolio/pyfolio.h>
#include <pyfolio/streaming/real_time_analyzer.h>
#include <iostream>
#include <random>
#include <thread>
#include <iomanip>

using namespace pyfolio;
using namespace pyfolio::streaming;

// Market data simulator for demonstration
class MarketDataSimulator {
private:
    std::mt19937 gen_;
    std::normal_distribution<double> return_dist_;
    Price current_price_;
    bool volatile_regime_;
    
public:
    MarketDataSimulator(Price initial_price = 100.0) 
        : gen_(std::random_device{}()), 
          return_dist_(0.0001, 0.015),  // ~2.5% annual return, 24% volatility
          current_price_(initial_price),
          volatile_regime_(false) {}
    
    Price generate_price() {
        // Simulate regime-dependent returns
        double mean = volatile_regime_ ? -0.0002 : 0.0001;
        double vol = volatile_regime_ ? 0.025 : 0.015;
        std::normal_distribution<double> dist(mean, vol);
        
        double ret = dist(gen_);
        current_price_ *= (1 + ret);
        
        // Occasionally switch regimes
        if (std::uniform_real_distribution<>(0, 1)(gen_) < 0.01) {
            volatile_regime_ = !volatile_regime_;
        }
        
        return current_price_;
    }
    
    Trade generate_trade(const std::string& symbol) {
        // Simulate random trades
        std::uniform_int_distribution<> qty_dist(100, 1000);
        std::uniform_real_distribution<> side_dist(0, 1);
        
        return Trade{
            symbol,
            static_cast<Shares>(qty_dist(gen_)),
            current_price_,
            side_dist(gen_) > 0.5 ? TransactionSide::Buy : TransactionSide::Sell,
            DateTime()
        };
    }
};

// Console dashboard for real-time display
class ConsoleDashboard {
private:
    mutable std::mutex display_mutex_;
    
public:
    void display_metrics(const analytics::PerformanceMetrics& metrics) {
        std::lock_guard<std::mutex> lock(display_mutex_);
        
        std::cout << "\n=== Performance Metrics Update ===" << std::endl;
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "Total Return: " << metrics.total_return * 100 << "%" << std::endl;
        std::cout << "Sharpe Ratio: " << metrics.sharpe_ratio << std::endl;
        std::cout << "Max Drawdown: " << metrics.max_drawdown * 100 << "%" << std::endl;
        std::cout << "Volatility: " << metrics.annual_volatility * 100 << "%" << std::endl;
        std::cout << "VaR (95%): " << metrics.var_95 * 100 << "%" << std::endl;
    }
    
    void display_alert(const RiskAlert& alert) {
        std::lock_guard<std::mutex> lock(display_mutex_);
        
        std::cout << "\n🚨 RISK ALERT: " << alert.alert_type << std::endl;
        std::cout << "Severity: " << alert.severity << std::endl;
        std::cout << "Message: " << alert.message << std::endl;
        
        for (const auto& [key, value] : alert.metrics) {
            std::cout << "  " << key << ": " << value << std::endl;
        }
    }
    
    void display_regime_change(analytics::RegimeType regime, double confidence) {
        std::lock_guard<std::mutex> lock(display_mutex_);
        
        analytics::RegimeCharacteristics temp{regime, 0, 0, 0, 0, ""};
        std::cout << "\n📊 REGIME CHANGE: " << temp.name() 
                  << " (confidence: " << confidence * 100 << "%)" << std::endl;
    }
    
    void display_position_update(const std::string& symbol, const Position& position) {
        std::lock_guard<std::mutex> lock(display_mutex_);
        
        std::cout << "\n💼 Position Update - " << symbol 
                  << ": " << position.shares << " shares @ $" 
                  << position.price << std::endl;
    }
    
    void display_statistics(const IncrementalStatistics& stats) {
        std::lock_guard<std::mutex> lock(display_mutex_);
        
        std::cout << "\n=== Incremental Statistics ===" << std::endl;
        std::cout << "Count: " << stats.count() << std::endl;
        std::cout << "Mean Return: " << stats.mean() * 100 << "%" << std::endl;
        std::cout << "Std Dev: " << stats.std_dev() * 100 << "%" << std::endl;
        std::cout << "Skewness: " << stats.skewness() << std::endl;
        std::cout << "Kurtosis: " << stats.kurtosis() << std::endl;
        std::cout << "Min: " << stats.min() * 100 << "%" << std::endl;
        std::cout << "Max: " << stats.max() * 100 << "%" << std::endl;
    }
};

int main() {
    std::cout << "=== Real-Time Streaming Analysis Example ===" << std::endl;
    
    // Configure streaming analyzer
    StreamingConfig config;
    config.buffer_size = 10000;
    config.lookback_window = 100;  // Keep last 100 observations
    config.update_frequency_ms = 1000;  // Update metrics every second
    config.risk_alert_threshold = 0.95;
    config.enable_regime_detection = true;
    config.enable_incremental_stats = true;
    
    // Create analyzer and dashboard
    auto analyzer = std::make_shared<RealTimeAnalyzer>(config);
    ConsoleDashboard dashboard;
    
    // Register event handlers
    analyzer->on_event(StreamEventType::PerformanceUpdate, 
        [&dashboard](const StreamEvent& event) {
            auto metrics = std::get<analytics::PerformanceMetrics>(event.data);
            dashboard.display_metrics(metrics);
        });
    
    analyzer->on_event(StreamEventType::RiskAlert,
        [&dashboard](const StreamEvent& event) {
            auto alert = std::get<RiskAlert>(event.data);
            dashboard.display_alert(alert);
        });
    
    analyzer->on_event(StreamEventType::RegimeChange,
        [&dashboard](const StreamEvent& event) {
            auto regime = std::get<analytics::RegimeType>(event.data);
            double confidence = std::stod(event.metadata.at("confidence"));
            dashboard.display_regime_change(regime, confidence);
        });
    
    analyzer->on_event(StreamEventType::PositionUpdate,
        [&dashboard](const StreamEvent& event) {
            auto position = std::get<Position>(event.data);
            dashboard.display_position_update(event.symbol, position);
        });
    
    // Start the analyzer
    auto start_result = analyzer->start();
    if (start_result.is_error()) {
        std::cerr << "Failed to start analyzer: " << start_result.error().message << std::endl;
        return 1;
    }
    
    std::cout << "✅ Real-time analyzer started" << std::endl;
    std::cout << "Simulating market data stream..." << std::endl;
    
    // Create market data simulator
    MarketDataSimulator simulator(100.0);
    
    // Simulate streaming data
    const std::string symbol = "AAPL";
    const int simulation_seconds = 30;
    const int ticks_per_second = 10;
    
    std::cout << "\nStreaming " << ticks_per_second << " ticks/second for " 
              << simulation_seconds << " seconds..." << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    int tick_count = 0;
    int trade_count = 0;
    
    while (true) {
        // Generate and push price update
        Price new_price = simulator.generate_price();
        analyzer->push_price(symbol, new_price);
        tick_count++;
        
        // Occasionally generate trades (10% chance)
        static std::mt19937 trade_gen(std::random_device{}());
        if (std::uniform_real_distribution<>(0, 1)(trade_gen) < 0.1) {
            Trade trade = simulator.generate_trade(symbol);
            analyzer->push_trade(trade);
            trade_count++;
        }
        
        // Sleep to simulate real-time data
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / ticks_per_second));
        
        // Check if simulation time is complete
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time).count();
        
        if (elapsed >= simulation_seconds) {
            break;
        }
        
        // Display statistics every 5 seconds
        if (tick_count % (ticks_per_second * 5) == 0) {
            dashboard.display_statistics(analyzer->get_return_statistics());
        }
    }
    
    std::cout << "\n=== Simulation Complete ===" << std::endl;
    std::cout << "Total ticks processed: " << tick_count << std::endl;
    std::cout << "Total trades executed: " << trade_count << std::endl;
    
    // Display final metrics
    auto final_metrics = analyzer->get_latest_metrics();
    if (final_metrics.is_ok()) {
        dashboard.display_metrics(final_metrics.value());
    }
    
    // Display final statistics
    dashboard.display_statistics(analyzer->get_return_statistics());
    
    // Get final VaR
    auto var_result = analyzer->get_current_var(0.95);
    if (var_result.is_ok()) {
        std::cout << "\nFinal VaR (95%): " << var_result.value() * 100 << "%" << std::endl;
    }
    
    // Get current regime
    auto regime_result = analyzer->get_current_regime();
    if (regime_result.is_ok()) {
        auto [regime, confidence] = regime_result.value();
        analytics::RegimeCharacteristics temp{regime, 0, 0, 0, 0, ""};
        std::cout << "Current Regime: " << temp.name() 
                  << " (confidence: " << confidence * 100 << "%)" << std::endl;
    }
    
    // Display positions
    auto positions = analyzer->get_positions();
    if (!positions.empty()) {
        std::cout << "\n=== Final Positions ===" << std::endl;
        for (const auto& [sym, pos] : positions) {
            std::cout << sym << ": " << pos.shares << " shares @ $" << pos.price << std::endl;
        }
    }
    
    // WebSocket example (placeholder)
    std::cout << "\n=== WebSocket Integration Example ===" << std::endl;
    WebSocketStreamer ws_client("wss://market-data.example.com", analyzer);
    
    auto connect_result = ws_client.connect();
    if (connect_result.is_ok()) {
        std::cout << "✅ WebSocket connected (simulated)" << std::endl;
        std::cout << "In production, this would stream real market data" << std::endl;
        
        // Simulate WebSocket message
        ws_client.on_message("{\"symbol\":\"AAPL\",\"price\":150.25,\"timestamp\":\"2024-01-01T12:00:00Z\"}");
        
        ws_client.disconnect();
        std::cout << "✅ WebSocket disconnected" << std::endl;
    }
    
    // Stop the analyzer
    analyzer->stop();
    std::cout << "\n✅ Real-time analyzer stopped" << std::endl;
    
    std::cout << "\n=== Key Features Demonstrated ===" << std::endl;
    std::cout << "1. Real-time price and trade processing" << std::endl;
    std::cout << "2. Incremental statistics calculation (Welford's algorithm)" << std::endl;
    std::cout << "3. Event-driven architecture with handlers" << std::endl;
    std::cout << "4. Risk alerts and VaR monitoring" << std::endl;
    std::cout << "5. Real-time regime detection" << std::endl;
    std::cout << "6. Position tracking and updates" << std::endl;
    std::cout << "7. Thread-safe concurrent processing" << std::endl;
    std::cout << "8. WebSocket integration framework" << std::endl;
    
    return 0;
}