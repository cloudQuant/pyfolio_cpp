#pragma once

#include "../core/datetime.h"
#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include "../analytics/performance_metrics.h"
#include "../analytics/regime_detection.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <queue>

namespace pyfolio::streaming {

/**
 * @brief Event types for streaming analysis
 */
enum class StreamEventType {
    PriceUpdate,
    TradeExecution,
    PositionUpdate,
    RiskAlert,
    RegimeChange,
    PerformanceUpdate,
    SystemStatus
};

/**
 * @brief Trade data for streaming
 */
struct Trade {
    std::string symbol;
    Shares quantity;
    Price price;
    TransactionSide side;
    DateTime timestamp;
};

/**
 * @brief Risk alert data
 */
struct RiskAlert {
    std::string alert_type;
    double severity;  // 0.0 to 1.0
    std::string message;
    std::unordered_map<std::string, double> metrics;
};

/**
 * @brief Streaming data event
 */
struct StreamEvent {
    StreamEventType type;
    DateTime timestamp;
    std::string symbol;
    
    // Event-specific data
    std::variant<Price, Trade, Position, RiskAlert, analytics::RegimeType, analytics::PerformanceMetrics> data;
    
    // Additional metadata
    std::unordered_map<std::string, std::string> metadata;
};

/**
 * @brief Configuration for real-time analysis
 */
struct StreamingConfig {
    size_t buffer_size = 10000;           ///< Maximum events in buffer
    size_t lookback_window = 252;         ///< Days of history to maintain
    size_t update_frequency_ms = 100;     ///< Update metrics every N milliseconds
    double risk_alert_threshold = 0.95;   ///< VaR threshold for alerts
    bool enable_regime_detection = true;  ///< Real-time regime detection
    bool enable_incremental_stats = true; ///< Use incremental statistics
    size_t thread_pool_size = 4;          ///< Worker threads for analysis
};

/**
 * @brief Incremental statistics calculator
 * 
 * Maintains running statistics without storing all historical data.
 * Uses Welford's algorithm for numerical stability.
 */
class IncrementalStatistics {
private:
    size_t count_ = 0;
    double mean_ = 0.0;
    double m2_ = 0.0;  // Sum of squared deviations
    double min_ = std::numeric_limits<double>::max();
    double max_ = std::numeric_limits<double>::lowest();
    
    // For higher moments
    double m3_ = 0.0;  // For skewness
    double m4_ = 0.0;  // For kurtosis
    
public:
    void update(double value) {
        count_++;
        
        double delta = value - mean_;
        double delta_n = delta / count_;
        double delta_n2 = delta_n * delta_n;
        double term1 = delta * delta_n * (count_ - 1);
        
        mean_ += delta_n;
        m4_ += term1 * delta_n2 * (count_ * count_ - 3 * count_ + 3) + 
               6 * delta_n2 * m2_ - 4 * delta_n * m3_;
        m3_ += term1 * delta_n * (count_ - 2) - 3 * delta_n * m2_;
        m2_ += term1;
        
        min_ = std::min(min_, value);
        max_ = std::max(max_, value);
    }
    
    double mean() const { return mean_; }
    double variance() const { return count_ > 1 ? m2_ / (count_ - 1) : 0.0; }
    double std_dev() const { return std::sqrt(variance()); }
    double skewness() const {
        if (count_ < 3 || m2_ == 0) return 0.0;
        return std::sqrt(count_) * m3_ / std::pow(m2_, 1.5);
    }
    double kurtosis() const {
        if (count_ < 4 || m2_ == 0) return 3.0;
        return count_ * m4_ / (m2_ * m2_) - 3.0;
    }
    double min() const { return count_ > 0 ? min_ : 0.0; }
    double max() const { return count_ > 0 ? max_ : 0.0; }
    size_t count() const { return count_; }
    
    void reset() {
        count_ = 0;
        mean_ = 0.0;
        m2_ = 0.0;
        m3_ = 0.0;
        m4_ = 0.0;
        min_ = std::numeric_limits<double>::max();
        max_ = std::numeric_limits<double>::lowest();
    }
};

/**
 * @brief Real-time portfolio analyzer
 * 
 * Processes streaming market data and provides real-time analytics including:
 * - Incremental performance metrics
 * - Real-time risk monitoring
 * - Regime detection
 * - Event-driven alerts
 */
class RealTimeAnalyzer {
private:
    StreamingConfig config_;
    std::atomic<bool> running_{false};
    
    // Thread management
    std::thread processing_thread_;
    std::vector<std::thread> worker_threads_;
    std::condition_variable cv_;
    std::mutex mutex_;
    
    // Event queue
    std::queue<StreamEvent> event_queue_;
    
    // Data storage (circular buffers)
    std::deque<DateTime> timestamps_;
    std::deque<Return> returns_;
    std::deque<Price> prices_;
    std::unordered_map<std::string, Position> current_positions_;
    
    // Incremental statistics
    IncrementalStatistics return_stats_;
    IncrementalStatistics price_stats_;
    
    // Cached metrics
    mutable std::mutex metrics_mutex_;
    analytics::PerformanceMetrics latest_metrics_;
    std::chrono::steady_clock::time_point last_update_;
    
    // Event handlers
    using EventHandler = std::function<void(const StreamEvent&)>;
    std::unordered_map<StreamEventType, std::vector<EventHandler>> event_handlers_;
    
    // Components
    std::unique_ptr<analytics::MLRegimeDetector> regime_detector_;
    
public:
    explicit RealTimeAnalyzer(const StreamingConfig& config = StreamingConfig())
        : config_(config) {
        if (config_.enable_regime_detection) {
            regime_detector_ = std::make_unique<analytics::MLRegimeDetector>(
                config_.lookback_window, 3);
        }
    }
    
    ~RealTimeAnalyzer() {
        stop();
    }
    
    /**
     * @brief Start the real-time analysis engine
     */
    Result<void> start() {
        if (running_.exchange(true)) {
            return Result<void>::error(ErrorCode::InvalidInput, 
                "Analyzer is already running");
        }
        
        // Start processing thread
        processing_thread_ = std::thread(&RealTimeAnalyzer::process_events, this);
        
        // Start worker threads
        for (size_t i = 0; i < config_.thread_pool_size; ++i) {
            worker_threads_.emplace_back(&RealTimeAnalyzer::worker_loop, this);
        }
        
        return Result<void>::success();
    }
    
    /**
     * @brief Stop the analysis engine
     */
    void stop() {
        if (!running_.exchange(false)) {
            return;
        }
        
        cv_.notify_all();
        
        if (processing_thread_.joinable()) {
            processing_thread_.join();
        }
        
        for (auto& thread : worker_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }
    
    /**
     * @brief Push new event to the stream
     */
    Result<void> push_event(StreamEvent event) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            if (event_queue_.size() >= config_.buffer_size) {
                return Result<void>::error(ErrorCode::NumericOverflow,
                    "Event queue is full");
            }
            
            event_queue_.push(std::move(event));
        }
        
        cv_.notify_one();
        return Result<void>::success();
    }
    
    /**
     * @brief Push price update
     */
    Result<void> push_price(const std::string& symbol, Price price, 
                           const DateTime& timestamp = DateTime()) {
        StreamEvent event{
            StreamEventType::PriceUpdate,
            timestamp,
            symbol,
            price,
            {}
        };
        
        return push_event(std::move(event));
    }
    
    /**
     * @brief Push trade execution
     */
    Result<void> push_trade(const Trade& trade) {
        StreamEvent event{
            StreamEventType::TradeExecution,
            trade.timestamp,
            trade.symbol,
            trade,
            {}
        };
        
        return push_event(std::move(event));
    }
    
    /**
     * @brief Register event handler
     */
    void on_event(StreamEventType type, EventHandler handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        event_handlers_[type].push_back(handler);
    }
    
    /**
     * @brief Get latest performance metrics
     */
    Result<analytics::PerformanceMetrics> get_latest_metrics() const {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        
        if (returns_.empty()) {
            return Result<analytics::PerformanceMetrics>::error(
                ErrorCode::InsufficientData,
                "No data available for metrics calculation");
        }
        
        return Result<analytics::PerformanceMetrics>::success(latest_metrics_);
    }
    
    /**
     * @brief Get current positions
     */
    std::unordered_map<std::string, Position> get_positions() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        return current_positions_;
    }
    
    /**
     * @brief Get incremental statistics
     */
    IncrementalStatistics get_return_statistics() const {
        return return_stats_;
    }
    
    /**
     * @brief Get real-time VaR
     */
    Result<double> get_current_var(double confidence_level = 0.95) const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        
        if (returns_.size() < 20) {
            return Result<double>::error(ErrorCode::InsufficientData,
                "Need at least 20 observations for VaR calculation");
        }
        
        // Convert deque to vector for VaR calculation
        std::vector<Return> recent_returns(returns_.begin(), returns_.end());
        std::vector<DateTime> recent_dates(timestamps_.begin(), timestamps_.end());
        
        auto ts = TimeSeries<Return>::create(recent_dates, recent_returns);
        if (ts.is_error()) {
            return Result<double>::error(ts.error().code, ts.error().message);
        }
        
        // Simple historical VaR calculation
        std::vector<Return> sorted_returns(recent_returns);
        std::sort(sorted_returns.begin(), sorted_returns.end());
        
        size_t index = static_cast<size_t>((1.0 - confidence_level) * sorted_returns.size());
        return Result<double>::success(-sorted_returns[index]);
    }
    
    /**
     * @brief Get current market regime
     */
    Result<std::pair<analytics::RegimeType, double>> get_current_regime() const {
        if (!regime_detector_) {
            return Result<std::pair<analytics::RegimeType, double>>::error(
                ErrorCode::InvalidInput,
                "Regime detection is not enabled");
        }
        
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        
        if (returns_.size() < regime_detector_->get_lookback_window()) {
            return Result<std::pair<analytics::RegimeType, double>>::error(
                ErrorCode::InsufficientData,
                "Insufficient data for regime detection");
        }
        
        // Get recent data for regime detection
        std::vector<Return> recent_returns(returns_.begin(), returns_.end());
        std::vector<DateTime> recent_dates(timestamps_.begin(), timestamps_.end());
        
        auto ts = TimeSeries<Return>::create(recent_dates, recent_returns);
        if (ts.is_error()) {
            return Result<std::pair<analytics::RegimeType, double>>::error(
                ts.error().code, ts.error().message);
        }
        
        return regime_detector_->detect_current_regime_adaptive(ts.value());
    }
    
private:
    /**
     * @brief Main event processing loop
     */
    void process_events() {
        while (running_) {
            std::unique_lock<std::mutex> lock(mutex_);
            
            cv_.wait(lock, [this] { 
                return !event_queue_.empty() || !running_; 
            });
            
            while (!event_queue_.empty() && running_) {
                StreamEvent event = std::move(event_queue_.front());
                event_queue_.pop();
                
                lock.unlock();
                handle_event(event);
                lock.lock();
            }
            
            // Check if metrics need updating
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_update_).count();
            
            if (elapsed >= config_.update_frequency_ms) {
                lock.unlock();
                update_metrics();
                lock.lock();
                last_update_ = now;
            }
        }
    }
    
    /**
     * @brief Worker thread loop for parallel processing
     */
    void worker_loop() {
        // Worker threads can be used for parallel metric calculations
        // Currently a placeholder for future expansion
        while (running_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    /**
     * @brief Handle individual event
     */
    void handle_event(const StreamEvent& event) {
        switch (event.type) {
            case StreamEventType::PriceUpdate:
                handle_price_update(event);
                break;
            case StreamEventType::TradeExecution:
                handle_trade_execution(event);
                break;
            case StreamEventType::PositionUpdate:
                handle_position_update(event);
                break;
            default:
                break;
        }
        
        // Trigger registered handlers
        auto it = event_handlers_.find(event.type);
        if (it != event_handlers_.end()) {
            for (const auto& handler : it->second) {
                handler(event);
            }
        }
    }
    
    /**
     * @brief Handle price update
     */
    void handle_price_update(const StreamEvent& event) {
        auto price = std::get<Price>(event.data);
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Update price history
        prices_.push_back(price);
        timestamps_.push_back(event.timestamp);
        
        // Calculate return if we have previous price
        if (prices_.size() > 1) {
            Return ret = (price - prices_[prices_.size() - 2]) / prices_[prices_.size() - 2];
            returns_.push_back(ret);
            
            // Update incremental statistics
            return_stats_.update(ret);
            price_stats_.update(price);
        }
        
        // Maintain buffer size
        while (prices_.size() > config_.lookback_window) {
            prices_.pop_front();
            timestamps_.pop_front();
            if (!returns_.empty()) {
                returns_.pop_front();
            }
        }
        
        // Check for risk alerts
        check_risk_alerts();
    }
    
    /**
     * @brief Handle trade execution
     */
    void handle_trade_execution(const StreamEvent& event) {
        auto trade = std::get<Trade>(event.data);
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Update positions
        auto& pos = current_positions_[trade.symbol];
        if (trade.side == TransactionSide::Buy) {
            pos.shares += trade.quantity;
        } else {
            pos.shares -= trade.quantity;
        }
        pos.price = trade.price;
        pos.timestamp = trade.timestamp.time_point();
        
        // Trigger position update event
        StreamEvent pos_event{
            StreamEventType::PositionUpdate,
            trade.timestamp,
            trade.symbol,
            pos,
            {}
        };
        
        // Note: We don't push to queue to avoid recursion
        // Instead, directly call handlers
        auto it = event_handlers_.find(StreamEventType::PositionUpdate);
        if (it != event_handlers_.end()) {
            for (const auto& handler : it->second) {
                handler(pos_event);
            }
        }
    }
    
    /**
     * @brief Handle position update
     */
    void handle_position_update(const StreamEvent& event) {
        auto position = std::get<Position>(event.data);
        
        std::lock_guard<std::mutex> lock(mutex_);
        current_positions_[event.symbol] = position;
    }
    
    /**
     * @brief Update performance metrics
     */
    void update_metrics() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (returns_.size() < 2) {
            return;
        }
        
        // Convert to TimeSeries for metric calculation
        std::vector<Return> ret_vec(returns_.begin(), returns_.end());
        std::vector<DateTime> date_vec(timestamps_.begin(), 
                                      timestamps_.begin() + returns_.size());
        
        auto ts_result = TimeSeries<Return>::create(date_vec, ret_vec);
        if (ts_result.is_error()) {
            return;
        }
        
        // Calculate basic metrics (simplified for demo)
        {
            std::lock_guard<std::mutex> metrics_lock(metrics_mutex_);
            
            // Calculate simple metrics using incremental stats
            latest_metrics_.total_return = return_stats_.mean() * 252; // Annualized
            latest_metrics_.annual_volatility = return_stats_.std_dev() * std::sqrt(252);
            latest_metrics_.sharpe_ratio = latest_metrics_.annual_volatility > 0 
                ? latest_metrics_.total_return / latest_metrics_.annual_volatility : 0.0;
            latest_metrics_.max_drawdown = 0.0; // Simplified
            latest_metrics_.var_95 = -return_stats_.mean() + 1.65 * return_stats_.std_dev();
            
            // Trigger performance update event
            StreamEvent perf_event{
                StreamEventType::PerformanceUpdate,
                DateTime(),
                "",
                latest_metrics_,
                {}
            };
            
            auto it = event_handlers_.find(StreamEventType::PerformanceUpdate);
            if (it != event_handlers_.end()) {
                for (const auto& handler : it->second) {
                    handler(perf_event);
                }
            }
        }
    }
    
    /**
     * @brief Check for risk alerts
     */
    void check_risk_alerts() {
        // Calculate current VaR
        auto var_result = get_current_var(config_.risk_alert_threshold);
        if (var_result.is_error()) {
            return;
        }
        
        double current_var = var_result.value();
        
        // Check if latest return exceeds VaR threshold
        if (!returns_.empty()) {
            double latest_return = returns_.back();
            
            if (latest_return < -current_var) {
                RiskAlert alert{
                    "VaR_Breach",
                    std::abs(latest_return / current_var),
                    "Return exceeded VaR threshold",
                    {{"var", current_var}, {"return", latest_return}}
                };
                
                StreamEvent alert_event{
                    StreamEventType::RiskAlert,
                    DateTime(),
                    "",
                    alert,
                    {}
                };
                
                auto it = event_handlers_.find(StreamEventType::RiskAlert);
                if (it != event_handlers_.end()) {
                    for (const auto& handler : it->second) {
                        handler(alert_event);
                    }
                }
            }
        }
        
        // Check for regime changes
        if (config_.enable_regime_detection && regime_detector_) {
            static analytics::RegimeType last_regime = analytics::RegimeType::Stable;
            
            auto regime_result = get_current_regime();
            if (regime_result.is_ok()) {
                auto [current_regime, confidence] = regime_result.value();
                
                if (current_regime != last_regime && confidence > 0.7) {
                    StreamEvent regime_event{
                        StreamEventType::RegimeChange,
                        DateTime(),
                        "",
                        current_regime,
                        {{"confidence", std::to_string(confidence)}}
                    };
                    
                    auto it = event_handlers_.find(StreamEventType::RegimeChange);
                    if (it != event_handlers_.end()) {
                        for (const auto& handler : it->second) {
                            handler(regime_event);
                        }
                    }
                    
                    last_regime = current_regime;
                }
            }
        }
    }
};

/**
 * @brief WebSocket streaming client (placeholder)
 * 
 * In production, this would integrate with websocket libraries
 * like websocketpp or Beast for real market data feeds.
 */
class WebSocketStreamer {
private:
    std::string url_;
    std::shared_ptr<RealTimeAnalyzer> analyzer_;
    std::atomic<bool> connected_{false};
    
public:
    WebSocketStreamer(const std::string& url, 
                     std::shared_ptr<RealTimeAnalyzer> analyzer)
        : url_(url), analyzer_(analyzer) {}
    
    Result<void> connect() {
        // Placeholder - would establish WebSocket connection
        connected_ = true;
        return Result<void>::success();
    }
    
    void disconnect() {
        connected_ = false;
    }
    
    bool is_connected() const {
        return connected_;
    }
    
    // In production, this would handle WebSocket messages
    void on_message(const std::string& message) {
        // Parse message and push to analyzer
        // Example: JSON parsing and event creation
    }
};

}  // namespace pyfolio::streaming