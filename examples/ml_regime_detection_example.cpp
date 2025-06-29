#include <pyfolio/pyfolio.h>
#include <pyfolio/analytics/regime_detection.h>
#include <iostream>
#include <random>

using namespace pyfolio;
using namespace pyfolio::analytics;

int main() {
    std::cout << "=== Machine Learning Regime Detection Example ===" << std::endl;
    
    // Generate sample market data with different regimes
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // Create synthetic data with three distinct regimes
    std::vector<DateTime> dates;
    std::vector<Return> returns;
    
    auto start_date = DateTime::parse("2020-01-01").value();
    
    // Regime 1: Bull market (high positive returns, low volatility)
    std::normal_distribution<double> bull_dist(0.0008, 0.015);  // ~20% annual, 15% vol
    for (int i = 0; i < 300; ++i) {
        dates.push_back(start_date.add_days(i));
        returns.push_back(bull_dist(gen));
    }
    
    // Regime 2: Crisis (negative returns, high volatility)
    std::normal_distribution<double> crisis_dist(-0.002, 0.035);  // -50% annual, 35% vol
    for (int i = 300; i < 450; ++i) {
        dates.push_back(start_date.add_days(i));
        returns.push_back(crisis_dist(gen));
    }
    
    // Regime 3: Recovery (moderate positive returns, medium volatility)
    std::normal_distribution<double> recovery_dist(0.0005, 0.022);  // 12% annual, 22% vol
    for (int i = 450; i < 750; ++i) {
        dates.push_back(start_date.add_days(i));
        returns.push_back(recovery_dist(gen));
    }
    
    // Create time series
    auto ts_result = TimeSeries<Return>::create(dates, returns, "Portfolio Returns");
    if (ts_result.is_error()) {
        std::cerr << "Error creating time series: " << ts_result.error().message << std::endl;
        return 1;
    }
    
    auto return_series = ts_result.value();
    std::cout << "Created synthetic return series with " << return_series.size() << " observations" << std::endl;
    
    // Initialize ML regime detector
    MLRegimeDetector detector(30, 3);  // 30-day lookback, 3 regimes
    
    std::cout << "\n=== Testing Different ML Regime Detection Methods ===" << std::endl;
    
    // 1. Deep Neural Network Detection
    std::cout << "\n1. Deep Neural Network Regime Detection:" << std::endl;
    auto dnn_result = detector.detect_regimes_dnn(return_series);
    if (dnn_result.is_ok()) {
        const auto& result = dnn_result.value();
        std::cout << "   - Detected " << result.regime_sequence.size() << " regime periods" << std::endl;
        
        // Show first few detections
        for (size_t i = 0; i < std::min(size_t(5), result.regime_sequence.size()); ++i) {
            RegimeCharacteristics temp{result.regime_sequence[i], 0, 0, 0, 0, ""};
            std::cout << "   - Period " << i+1 << ": " << temp.name() 
                      << " (prob: " << (i < result.regime_probabilities.size() ? result.regime_probabilities[i] : 0.0) << ")" << std::endl;
        }
    }
    
    // 2. Ensemble Detection
    std::cout << "\n2. Ensemble Regime Detection:" << std::endl;
    auto ensemble_result = detector.detect_regimes_ensemble(return_series);
    if (ensemble_result.is_ok()) {
        const auto& result = ensemble_result.value();
        std::cout << "   - Detected " << result.regime_sequence.size() << " regime periods" << std::endl;
        
        // Calculate regime statistics
        std::map<RegimeType, int> regime_counts;
        for (const auto& regime_type : result.regime_sequence) {
            regime_counts[regime_type]++;
        }
        
        std::cout << "   - Regime distribution:" << std::endl;
        for (const auto& [type, count] : regime_counts) {
            RegimeCharacteristics temp{type, 0, 0, 0, 0, ""};
            std::cout << "     * " << temp.name() << ": " << count << " periods" << std::endl;
        }
    }
    
    // 3. Random Forest Detection
    std::cout << "\n3. Random Forest Regime Detection:" << std::endl;
    auto rf_result = detector.detect_regimes_random_forest(return_series);
    if (rf_result.is_ok()) {
        const auto& result = rf_result.value();
        std::cout << "   - Detected " << result.regime_sequence.size() << " regime periods" << std::endl;
        
        // Calculate average regime persistence (simplified)
        if (!result.regime_sequence.empty()) {
            double avg_persistence = static_cast<double>(result.regime_sequence.size()) / 3.0; // Rough estimate
            std::cout << "   - Average regime persistence: " << avg_persistence << " days" << std::endl;
        }
    }
    
    // 4. Support Vector Machine Detection
    std::cout << "\n4. Support Vector Machine Regime Detection:" << std::endl;
    auto svm_result = detector.detect_regimes_svm(return_series);
    if (svm_result.is_ok()) {
        const auto& result = svm_result.value();
        std::cout << "   - Detected " << result.regime_sequence.size() << " regime periods" << std::endl;
        
        // Show risk levels
        std::map<int, int> risk_level_counts;
        for (const auto& regime_type : result.regime_sequence) {
            RegimeCharacteristics temp{regime_type, 0, 0, 0, 0, ""};
            risk_level_counts[temp.risk_level()]++;
        }
        
        std::cout << "   - Risk level distribution:" << std::endl;
        for (const auto& [level, count] : risk_level_counts) {
            std::cout << "     * Risk Level " << level << ": " << count << " periods" << std::endl;
        }
    }
    
    // 5. Adaptive Online Detection (simulate real-time)
    std::cout << "\n5. Adaptive Online Detection (Real-time Simulation):" << std::endl;
    
    // Simulate streaming data
    size_t window_start = 100;  // Start after some initial data
    size_t stream_size = 50;    // Process 50 new observations
    
    for (size_t i = 0; i < stream_size; ++i) {
        size_t end_idx = window_start + detector.get_lookback_window() + i;
        if (end_idx >= return_series.size()) break;
        
        // Create sliding window
        auto window_dates = std::vector<DateTime>(
            dates.begin() + window_start + i,
            dates.begin() + end_idx + 1
        );
        auto window_returns = std::vector<Return>(
            returns.begin() + window_start + i,
            returns.begin() + end_idx + 1
        );
        
        auto window_series = TimeSeries<Return>::create(window_dates, window_returns).value();
        
        // Detect current regime
        auto current_regime = detector.detect_current_regime_adaptive(window_series);
        if (current_regime.is_ok() && i % 10 == 0) {  // Print every 10th detection
            const auto& regime_pair = current_regime.value();
            RegimeCharacteristics temp{regime_pair.first, 0, 0, 0, 0, ""};
            std::cout << "   - Day " << window_start + detector.get_lookback_window() + i 
                      << ": " << temp.name() 
                      << " (confidence: " << regime_pair.second << ")" << std::endl;
        }
    }
    
    // 6. Feature Extraction Example
    std::cout << "\n6. Advanced Feature Extraction:" << std::endl;
    auto features_result = detector.extract_advanced_features(return_series);
    if (features_result.is_ok()) {
        const auto& features = features_result.value();
        std::cout << "   - Extracted " << features.size() << " features per observation" << std::endl;
        std::cout << "   - Features include: volatility, skewness, momentum, mean reversion, etc." << std::endl;
        
        // Show sample features from middle of dataset
        if (features.size() > 10) {
            size_t mid_idx = features.size() / 2;
            std::cout << "   - Sample features (observation " << mid_idx << "):" << std::endl;
            for (size_t i = 0; i < std::min(size_t(8), features[mid_idx].size()); ++i) {
                std::cout << "     * Feature " << i+1 << ": " << features[mid_idx][i] << std::endl;
            }
        }
    }
    
    // Performance Comparison
    std::cout << "\n=== Performance Analysis ===" << std::endl;
    
    // Compare detected regimes with actual synthetic regimes
    std::cout << "Note: In this synthetic example, we created:" << std::endl;
    std::cout << "  - Days 1-300: Bull market regime" << std::endl;
    std::cout << "  - Days 301-450: Crisis regime" << std::endl;
    std::cout << "  - Days 451-750: Recovery regime" << std::endl;
    std::cout << "\nML algorithms should ideally detect these three distinct periods." << std::endl;
    
    // Trading Strategy Implications
    std::cout << "\n=== Trading Strategy Implications ===" << std::endl;
    std::cout << "Based on detected regimes, suggested portfolio adjustments:" << std::endl;
    
    // Simulate using the last detected regime for strategy
    if (ensemble_result.is_ok() && !ensemble_result.value().regime_sequence.empty()) {
        const auto& result = ensemble_result.value();
        auto last_regime_type = result.regime_sequence.back();
        RegimeCharacteristics characteristics{last_regime_type, 0, 0, 0, 0, ""};
        
        std::cout << "Current Regime: " << characteristics.name() << std::endl;
        std::cout << "Risk Level: " << characteristics.risk_level() << "/5" << std::endl;
        
        if (characteristics.is_favorable_for_long()) {
            std::cout << "Strategy: Favorable for long positions" << std::endl;
            std::cout << "  - Consider increasing equity allocation" << std::endl;
            std::cout << "  - Reduce cash/bond holdings" << std::endl;
        } else {
            std::cout << "Strategy: Unfavorable for long positions" << std::endl;
            std::cout << "  - Consider defensive positioning" << std::endl;
            std::cout << "  - Increase cash/bond allocation" << std::endl;
            std::cout << "  - Consider hedging strategies" << std::endl;
        }
    }
    
    std::cout << "\n=== Machine Learning Regime Detection Complete ===" << std::endl;
    std::cout << "Note: This example uses placeholder ML algorithms." << std::endl;
    std::cout << "Production implementation would integrate with:" << std::endl;
    std::cout << "  - TensorFlow C++ API for deep learning" << std::endl;
    std::cout << "  - scikit-learn equivalent C++ libraries" << std::endl;
    std::cout << "  - mlpack for machine learning algorithms" << std::endl;
    std::cout << "  - Eigen for linear algebra operations" << std::endl;
    
    return 0;
}