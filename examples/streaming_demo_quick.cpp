#include <pyfolio/streaming/real_time_analyzer.h>
#include <iostream>
#include <random>
#include <thread>

using namespace pyfolio;
using namespace pyfolio::streaming;

int main() {
    std::cout << "=== Quick Streaming Analysis Demo ===" << std::endl;
    
    // Configure analyzer
    StreamingConfig config;
    config.buffer_size = 1000;
    config.lookback_window = 50;
    config.update_frequency_ms = 500;
    config.enable_regime_detection = false;  // Disable for quick demo
    
    RealTimeAnalyzer analyzer(config);
    
    // Register event handler
    analyzer.on_event(StreamEventType::PerformanceUpdate, 
        [](const StreamEvent& event) {
            auto metrics = std::get<pyfolio::analytics::PerformanceMetrics>(event.data);
            std::cout << "📈 Metrics Update: Sharpe=" << metrics.sharpe_ratio 
                      << ", Vol=" << metrics.annual_volatility * 100 << "%" << std::endl;
        });
    
    analyzer.on_event(StreamEventType::RiskAlert,
        [](const StreamEvent& event) {
            auto alert = std::get<RiskAlert>(event.data);
            std::cout << "🚨 Risk Alert: " << alert.alert_type 
                      << " (severity: " << alert.severity << ")" << std::endl;
        });
    
    // Start analyzer
    auto result = analyzer.start();
    if (result.is_error()) {
        std::cerr << "Failed to start: " << result.error().message << std::endl;
        return 1;
    }
    
    std::cout << "✅ Analyzer started" << std::endl;
    
    // Simulate price updates
    std::mt19937 gen(42);
    std::normal_distribution<double> dist(0.001, 0.02);
    double price = 100.0;
    
    for (int i = 0; i < 20; ++i) {
        price *= (1 + dist(gen));
        analyzer.push_price("DEMO", price);
        
        if (i % 5 == 0) {
            // Add a trade
            Trade trade{"DEMO", 100.0, price, TransactionSide::Buy, DateTime()};
            analyzer.push_trade(trade);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Wait for processing
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Get final statistics
    auto stats = analyzer.get_return_statistics();
    std::cout << "\n=== Final Statistics ===" << std::endl;
    std::cout << "Count: " << stats.count() << std::endl;
    std::cout << "Mean Return: " << stats.mean() * 100 << "%" << std::endl;
    std::cout << "Volatility: " << stats.std_dev() * 100 << "%" << std::endl;
    
    // Get positions
    auto positions = analyzer.get_positions();
    std::cout << "Positions: " << positions.size() << std::endl;
    
    analyzer.stop();
    std::cout << "✅ Demo complete" << std::endl;
    
    return 0;
}