#pragma once

#include "advanced_backtester.h"
#include "../analytics/statistics.h"
#include <deque>
#include <numeric>

namespace pyfolio::backtesting::strategies {

/**
 * @brief Buy and Hold strategy
 */
class BuyAndHoldStrategy : public TradingStrategy {
private:
    std::vector<std::string> symbols_;
    bool initialized_ = false;
    
public:
    explicit BuyAndHoldStrategy(const std::vector<std::string>& symbols)
        : symbols_(symbols) {}
    
    std::unordered_map<std::string, double> generate_signals(
        const DateTime& timestamp,
        const std::unordered_map<std::string, Price>& prices,
        const PortfolioState& portfolio) override {
        
        std::unordered_map<std::string, double> weights;
        
        if (!initialized_) {
            // Equal weight allocation
            double weight_per_symbol = 1.0 / symbols_.size();
            for (const auto& symbol : symbols_) {
                if (prices.find(symbol) != prices.end()) {
                    weights[symbol] = weight_per_symbol;
                }
            }
            initialized_ = true;
        } else {
            // Maintain current positions (no rebalancing)
            // Return weights based on current positions to signal holding
            for (const auto& [symbol, position] : portfolio.positions) {
                if (position.shares > 0) {
                    weights[symbol] = position.weight; // Use stored weight as signal to maintain position
                }
            }
        }
        
        return weights;
    }
    
    std::string get_name() const override {
        return "BuyAndHold";
    }
};

/**
 * @brief Mean Reversion strategy
 */
class MeanReversionStrategy : public TradingStrategy {
private:
    std::vector<std::string> symbols_;
    size_t lookback_period_;
    double rebalance_threshold_;
    
    // Price history for each symbol
    std::unordered_map<std::string, std::deque<Price>> price_history_;
    
public:
    MeanReversionStrategy(const std::vector<std::string>& symbols,
                         size_t lookback_period = 20,
                         double rebalance_threshold = 0.02)
        : symbols_(symbols), lookback_period_(lookback_period), 
          rebalance_threshold_(rebalance_threshold) {}
    
    std::unordered_map<std::string, double> generate_signals(
        const DateTime& timestamp,
        const std::unordered_map<std::string, Price>& prices,
        const PortfolioState& portfolio) override {
        
        std::unordered_map<std::string, double> weights;
        
        // Update price history
        for (const auto& symbol : symbols_) {
            auto price_it = prices.find(symbol);
            if (price_it != prices.end()) {
                auto& history = price_history_[symbol];
                history.push_back(price_it->second);
                
                // Maintain lookback window
                if (history.size() > lookback_period_) {
                    history.pop_front();
                }
            }
        }
        
        // Calculate mean reversion signals
        std::vector<double> z_scores;
        std::vector<std::string> valid_symbols;
        
        for (const auto& symbol : symbols_) {
            auto& history = price_history_[symbol];
            if (history.size() >= lookback_period_) {
                // Calculate mean and standard deviation
                double mean = std::accumulate(history.begin(), history.end(), 0.0) / history.size();
                
                double variance = 0.0;
                for (Price price : history) {
                    variance += (price - mean) * (price - mean);
                }
                variance /= (history.size() - 1);
                double std_dev = std::sqrt(variance);
                
                if (std_dev > 0) {
                    double current_price = history.back();
                    double z_score = (current_price - mean) / std_dev;
                    z_scores.push_back(-z_score); // Negative for mean reversion
                    valid_symbols.push_back(symbol);
                }
            }
        }
        
        if (!z_scores.empty()) {
            // Convert z-scores to weights using softmax
            std::vector<double> exp_scores;
            double max_score = *std::max_element(z_scores.begin(), z_scores.end());
            
            double sum_exp = 0.0;
            for (double score : z_scores) {
                double exp_score = std::exp(score - max_score);
                exp_scores.push_back(exp_score);
                sum_exp += exp_score;
            }
            
            // Normalize to weights
            for (size_t i = 0; i < valid_symbols.size(); ++i) {
                weights[valid_symbols[i]] = exp_scores[i] / sum_exp;
            }
        } else {
            // Fall back to equal weights
            double equal_weight = 1.0 / symbols_.size();
            for (const auto& symbol : symbols_) {
                if (prices.find(symbol) != prices.end()) {
                    weights[symbol] = equal_weight;
                }
            }
        }
        
        return weights;
    }
    
    std::string get_name() const override {
        return "MeanReversion";
    }
    
    std::unordered_map<std::string, double> get_parameters() const override {
        return {
            {"lookback_period", static_cast<double>(lookback_period_)},
            {"rebalance_threshold", rebalance_threshold_}
        };
    }
};

/**
 * @brief Momentum strategy
 */
class MomentumStrategy : public TradingStrategy {
private:
    std::vector<std::string> symbols_;
    size_t lookback_period_;
    size_t top_n_;
    
    std::unordered_map<std::string, std::deque<Price>> price_history_;
    
public:
    MomentumStrategy(const std::vector<std::string>& symbols,
                    size_t lookback_period = 60,
                    size_t top_n = 5)
        : symbols_(symbols), lookback_period_(lookback_period), top_n_(top_n) {}
    
    std::unordered_map<std::string, double> generate_signals(
        const DateTime& timestamp,
        const std::unordered_map<std::string, Price>& prices,
        const PortfolioState& portfolio) override {
        
        std::unordered_map<std::string, double> weights;
        
        // Update price history
        for (const auto& symbol : symbols_) {
            auto price_it = prices.find(symbol);
            if (price_it != prices.end()) {
                auto& history = price_history_[symbol];
                history.push_back(price_it->second);
                
                if (history.size() > lookback_period_) {
                    history.pop_front();
                }
            }
        }
        
        // Calculate momentum scores
        std::vector<std::pair<std::string, double>> momentum_scores;
        
        for (const auto& symbol : symbols_) {
            auto& history = price_history_[symbol];
            if (history.size() >= lookback_period_) {
                double start_price = history.front();
                double end_price = history.back();
                double momentum = (end_price - start_price) / start_price;
                momentum_scores.emplace_back(symbol, momentum);
            }
        }
        
        if (!momentum_scores.empty()) {
            // Sort by momentum (descending)
            std::sort(momentum_scores.begin(), momentum_scores.end(),
                     [](const auto& a, const auto& b) {
                         return a.second > b.second;
                     });
            
            // Select top N symbols
            size_t num_selected = std::min(top_n_, momentum_scores.size());
            double weight_per_symbol = 1.0 / num_selected;
            
            for (size_t i = 0; i < num_selected; ++i) {
                weights[momentum_scores[i].first] = weight_per_symbol;
            }
        } else {
            // Fall back to equal weights
            double equal_weight = 1.0 / symbols_.size();
            for (const auto& symbol : symbols_) {
                if (prices.find(symbol) != prices.end()) {
                    weights[symbol] = equal_weight;
                }
            }
        }
        
        return weights;
    }
    
    std::string get_name() const override {
        return "Momentum";
    }
    
    std::unordered_map<std::string, double> get_parameters() const override {
        return {
            {"lookback_period", static_cast<double>(lookback_period_)},
            {"top_n", static_cast<double>(top_n_)}
        };
    }
};

/**
 * @brief Equal Weight Rebalancing strategy
 */
class EqualWeightStrategy : public TradingStrategy {
private:
    std::vector<std::string> symbols_;
    size_t rebalance_frequency_; // Days between rebalancing
    size_t days_since_rebalance_ = 0;
    
public:
    EqualWeightStrategy(const std::vector<std::string>& symbols,
                       size_t rebalance_frequency = 21) // Monthly rebalancing
        : symbols_(symbols), rebalance_frequency_(rebalance_frequency) {}
    
    std::unordered_map<std::string, double> generate_signals(
        const DateTime& timestamp,
        const std::unordered_map<std::string, Price>& prices,
        const PortfolioState& portfolio) override {
        
        std::unordered_map<std::string, double> weights;
        
        days_since_rebalance_++;
        
        if (days_since_rebalance_ >= rebalance_frequency_ || portfolio.positions.empty()) {
            // Rebalance to equal weights
            size_t num_available = 0;
            for (const auto& symbol : symbols_) {
                if (prices.find(symbol) != prices.end()) {
                    num_available++;
                }
            }
            
            if (num_available > 0) {
                double weight_per_symbol = 1.0 / num_available;
                for (const auto& symbol : symbols_) {
                    if (prices.find(symbol) != prices.end()) {
                        weights[symbol] = weight_per_symbol;
                    }
                }
            }
            
            days_since_rebalance_ = 0;
        } else {
            // Maintain current weights
            auto current_weights = portfolio.get_weights();
            for (const auto& [symbol, weight] : current_weights) {
                weights[symbol] = weight;
            }
        }
        
        return weights;
    }
    
    std::string get_name() const override {
        return "EqualWeight";
    }
    
    std::unordered_map<std::string, double> get_parameters() const override {
        return {
            {"rebalance_frequency", static_cast<double>(rebalance_frequency_)}
        };
    }
};

/**
 * @brief Risk Parity strategy (simplified version)
 */
class RiskParityStrategy : public TradingStrategy {
private:
    std::vector<std::string> symbols_;
    size_t volatility_lookback_;
    size_t rebalance_frequency_;
    size_t days_since_rebalance_ = 0;
    
    std::unordered_map<std::string, std::deque<double>> return_history_;
    
public:
    RiskParityStrategy(const std::vector<std::string>& symbols,
                      size_t volatility_lookback = 60,
                      size_t rebalance_frequency = 21)
        : symbols_(symbols), volatility_lookback_(volatility_lookback),
          rebalance_frequency_(rebalance_frequency) {}
    
    std::unordered_map<std::string, double> generate_signals(
        const DateTime& timestamp,
        const std::unordered_map<std::string, Price>& prices,
        const PortfolioState& portfolio) override {
        
        std::unordered_map<std::string, double> weights;
        
        // Calculate returns and update history
        for (const auto& symbol : symbols_) {
            auto price_it = prices.find(symbol);
            if (price_it != prices.end()) {
                auto& history = return_history_[symbol];
                
                if (!history.empty()) {
                    // Calculate return from last period (assuming we have stored prices)
                    // For simplicity, we'll use a placeholder return calculation
                    double return_val = 0.001 * (std::rand() % 100 - 50) / 50.0; // Random walk
                    history.push_back(return_val);
                    
                    if (history.size() > volatility_lookback_) {
                        history.pop_front();
                    }
                } else {
                    history.push_back(0.0); // Initial return
                }
            }
        }
        
        days_since_rebalance_++;
        
        if (days_since_rebalance_ >= rebalance_frequency_ || portfolio.positions.empty()) {
            // Calculate volatilities and inverse volatility weights
            std::vector<std::pair<std::string, double>> volatilities;
            
            for (const auto& symbol : symbols_) {
                auto& history = return_history_[symbol];
                if (history.size() >= 10) { // Minimum data
                    // Calculate volatility
                    double mean = std::accumulate(history.begin(), history.end(), 0.0) / history.size();
                    double variance = 0.0;
                    for (double ret : history) {
                        variance += (ret - mean) * (ret - mean);
                    }
                    variance /= (history.size() - 1);
                    double volatility = std::sqrt(variance);
                    
                    if (volatility > 0) {
                        volatilities.emplace_back(symbol, 1.0 / volatility); // Inverse volatility
                    }
                }
            }
            
            if (!volatilities.empty()) {
                // Normalize weights
                double total_inv_vol = 0.0;
                for (const auto& [symbol, inv_vol] : volatilities) {
                    total_inv_vol += inv_vol;
                }
                
                for (const auto& [symbol, inv_vol] : volatilities) {
                    weights[symbol] = inv_vol / total_inv_vol;
                }
            } else {
                // Fall back to equal weights
                size_t num_available = 0;
                for (const auto& symbol : symbols_) {
                    if (prices.find(symbol) != prices.end()) {
                        num_available++;
                    }
                }
                
                if (num_available > 0) {
                    double weight_per_symbol = 1.0 / num_available;
                    for (const auto& symbol : symbols_) {
                        if (prices.find(symbol) != prices.end()) {
                            weights[symbol] = weight_per_symbol;
                        }
                    }
                }
            }
            
            days_since_rebalance_ = 0;
        } else {
            // Maintain current weights
            auto current_weights = portfolio.get_weights();
            for (const auto& [symbol, weight] : current_weights) {
                weights[symbol] = weight;
            }
        }
        
        return weights;
    }
    
    std::string get_name() const override {
        return "RiskParity";
    }
    
    std::unordered_map<std::string, double> get_parameters() const override {
        return {
            {"volatility_lookback", static_cast<double>(volatility_lookback_)},
            {"rebalance_frequency", static_cast<double>(rebalance_frequency_)}
        };
    }
};

/**
 * @brief Minimum Variance strategy
 */
class MinimumVarianceStrategy : public TradingStrategy {
private:
    std::vector<std::string> symbols_;
    size_t lookback_period_;
    size_t rebalance_frequency_;
    size_t days_since_rebalance_ = 0;
    
    std::unordered_map<std::string, std::deque<double>> return_history_;
    
public:
    MinimumVarianceStrategy(const std::vector<std::string>& symbols,
                           size_t lookback_period = 120,
                           size_t rebalance_frequency = 21)
        : symbols_(symbols), lookback_period_(lookback_period),
          rebalance_frequency_(rebalance_frequency) {}
    
    std::unordered_map<std::string, double> generate_signals(
        const DateTime& timestamp,
        const std::unordered_map<std::string, Price>& prices,
        const PortfolioState& portfolio) override {
        
        std::unordered_map<std::string, double> weights;
        
        // Update return history (simplified)
        for (const auto& symbol : symbols_) {
            auto price_it = prices.find(symbol);
            if (price_it != prices.end()) {
                auto& history = return_history_[symbol];
                
                // Simplified return calculation
                double return_val = 0.001 * (std::rand() % 100 - 50) / 50.0;
                history.push_back(return_val);
                
                if (history.size() > lookback_period_) {
                    history.pop_front();
                }
            }
        }
        
        days_since_rebalance_++;
        
        if (days_since_rebalance_ >= rebalance_frequency_ || portfolio.positions.empty()) {
            // Simplified minimum variance: equal weight for now
            // In practice, this would solve a quadratic optimization problem
            size_t num_available = 0;
            for (const auto& symbol : symbols_) {
                if (prices.find(symbol) != prices.end()) {
                    num_available++;
                }
            }
            
            if (num_available > 0) {
                double weight_per_symbol = 1.0 / num_available;
                for (const auto& symbol : symbols_) {
                    if (prices.find(symbol) != prices.end()) {
                        weights[symbol] = weight_per_symbol;
                    }
                }
            }
            
            days_since_rebalance_ = 0;
        } else {
            // Maintain current weights
            auto current_weights = portfolio.get_weights();
            for (const auto& [symbol, weight] : current_weights) {
                weights[symbol] = weight;
            }
        }
        
        return weights;
    }
    
    std::string get_name() const override {
        return "MinimumVariance";
    }
    
    std::unordered_map<std::string, double> get_parameters() const override {
        return {
            {"lookback_period", static_cast<double>(lookback_period_)},
            {"rebalance_frequency", static_cast<double>(rebalance_frequency_)}
        };
    }
};

}  // namespace pyfolio::backtesting::strategies