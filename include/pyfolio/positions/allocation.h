#pragma once

#include "../core/dataframe.h"
#include "../core/datetime.h"
#include "../core/error_handling.h"
#include "../core/types.h"
#include "holdings.h"
#include <map>
#include <numeric>
#include <set>

namespace pyfolio::positions {

/**
 * @brief Sector allocation information
 */
struct SectorAllocation {
    std::string sector;
    Weight weight;
    double market_value;
    int num_positions;
    std::vector<Symbol> symbols;
};

/**
 * @brief Concentration metrics
 */
struct ConcentrationMetrics {
    double herfindahl_index;      // Sum of squared weights
    double top_5_concentration;   // Weight of top 5 positions
    double top_10_concentration;  // Weight of top 10 positions
    double gini_coefficient;      // Inequality measure
    int effective_positions;      // 1 / herfindahl_index
};

/**
 * @brief Allocation analyzer
 */
class AllocationAnalyzer {
  private:
    std::map<Symbol, std::string> symbol_to_sector_;
    std::map<Symbol, std::string> symbol_to_industry_;
    std::map<Symbol, std::string> symbol_to_country_;

  public:
    /**
     * @brief Set sector mapping
     */
    void set_sector_mapping(const std::map<Symbol, std::string>& mapping) { symbol_to_sector_ = mapping; }

    /**
     * @brief Set industry mapping
     */
    void set_industry_mapping(const std::map<Symbol, std::string>& mapping) { symbol_to_industry_ = mapping; }

    /**
     * @brief Set country mapping
     */
    void set_country_mapping(const std::map<Symbol, std::string>& mapping) { symbol_to_country_ = mapping; }

    /**
     * @brief Calculate sector allocations
     */
    Result<std::vector<SectorAllocation>> calculate_sector_allocations(const PortfolioHoldings& holdings) const {
        if (holdings.holdings().empty()) {
            return Result<std::vector<SectorAllocation>>::error(ErrorCode::InsufficientData, "No holdings to analyze");
        }

        // Aggregate by sector
        std::map<std::string, SectorAllocation> sector_map;

        for (const auto& [symbol, holding] : holdings.holdings()) {
            std::string sector = "Unknown";

            auto it = symbol_to_sector_.find(symbol);
            if (it != symbol_to_sector_.end()) {
                sector = it->second;
            }

            auto& alloc  = sector_map[sector];
            alloc.sector = sector;
            alloc.weight += holding.weight;
            alloc.market_value += holding.market_value;
            alloc.num_positions++;
            alloc.symbols.push_back(symbol);
        }

        // Convert to vector and sort by weight
        std::vector<SectorAllocation> result;
        for (const auto& [sector, alloc] : sector_map) {
            result.push_back(alloc);
        }

        std::sort(result.begin(), result.end(), [](const SectorAllocation& a, const SectorAllocation& b) {
            return std::abs(a.weight) > std::abs(b.weight);
        });

        return Result<std::vector<SectorAllocation>>::success(std::move(result));
    }

    /**
     * @brief Calculate concentration metrics
     */
    Result<ConcentrationMetrics> calculate_concentration(const PortfolioHoldings& holdings) const {
        if (holdings.holdings().empty()) {
            return Result<ConcentrationMetrics>::error(ErrorCode::InsufficientData, "No holdings to analyze");
        }

        ConcentrationMetrics metrics{};

        // Get sorted weights
        std::vector<double> weights;
        for (const auto& [symbol, holding] : holdings.holdings()) {
            weights.push_back(std::abs(holding.weight));
        }

        std::sort(weights.begin(), weights.end(), std::greater<double>());

        // Herfindahl index
        metrics.herfindahl_index =
            std::accumulate(weights.begin(), weights.end(), 0.0, [](double sum, double w) { return sum + w * w; });

        // Top N concentrations
        if (weights.size() >= 5) {
            metrics.top_5_concentration = std::accumulate(weights.begin(), weights.begin() + 5, 0.0);
        } else {
            metrics.top_5_concentration = std::accumulate(weights.begin(), weights.end(), 0.0);
        }

        if (weights.size() >= 10) {
            metrics.top_10_concentration = std::accumulate(weights.begin(), weights.begin() + 10, 0.0);
        } else {
            metrics.top_10_concentration = std::accumulate(weights.begin(), weights.end(), 0.0);
        }

        // Effective positions
        if (metrics.herfindahl_index > 0) {
            metrics.effective_positions = static_cast<int>(1.0 / metrics.herfindahl_index);
        }

        // Gini coefficient
        metrics.gini_coefficient = calculate_gini_coefficient(weights);

        return Result<ConcentrationMetrics>::success(metrics);
    }

    /**
     * @brief Analyze allocation by custom grouping
     */
    template <typename GroupFunc>
    Result<DataFrame> analyze_by_group(const PortfolioHoldings& holdings, GroupFunc group_func,
                                       const std::string& group_name) const {
        if (holdings.holdings().empty()) {
            return Result<DataFrame>::error(ErrorCode::InsufficientData, "No holdings to analyze");
        }

        // Group holdings
        std::map<std::string, std::vector<const Holding*>> groups;

        for (const auto& [symbol, holding] : holdings.holdings()) {
            std::string group = group_func(symbol, holding);
            groups[group].push_back(&holding);
        }

        // Calculate group statistics
        std::vector<DateTime> timestamps;
        std::vector<std::string> group_names;
        std::vector<double> weights;
        std::vector<double> market_values;
        std::vector<int> counts;

        for (const auto& [group, holdings_vec] : groups) {
            timestamps.push_back(holdings.timestamp());
            group_names.push_back(group);

            double group_weight = 0.0;
            double group_value  = 0.0;

            for (const auto* h : holdings_vec) {
                group_weight += h->weight;
                group_value += h->market_value;
            }

            weights.push_back(group_weight);
            market_values.push_back(group_value);
            counts.push_back(static_cast<int>(holdings_vec.size()));
        }

        DataFrame df{timestamps};
        df.add_column(group_name, group_names);
        df.add_column("weight", weights);
        df.add_column("market_value", market_values);
        df.add_column("count", counts);

        return Result<DataFrame>::success(std::move(df));
    }

    /**
     * @brief Calculate allocation drift from target
     */
    struct AllocationDrift {
        Symbol symbol;
        Weight current_weight;
        Weight target_weight;
        Weight drift;
        double rebalance_shares;
        double rebalance_value;
    };

    Result<std::vector<AllocationDrift>> calculate_drift(const PortfolioHoldings& holdings,
                                                         const std::map<Symbol, Weight>& target_weights) const {
        std::vector<AllocationDrift> drifts;
        std::set<Symbol> all_symbols;

        // Get all symbols
        for (const auto& [symbol, holding] : holdings.holdings()) {
            all_symbols.insert(symbol);
        }
        for (const auto& [symbol, weight] : target_weights) {
            all_symbols.insert(symbol);
        }

        // Calculate drift for each symbol
        for (const auto& symbol : all_symbols) {
            AllocationDrift drift;
            drift.symbol = symbol;

            // Get current weight
            auto holding_result = holdings.get_holding(symbol);
            if (holding_result.is_ok()) {
                drift.current_weight = holding_result.value().weight;
            } else {
                drift.current_weight = 0.0;
            }

            // Get target weight
            auto target_it      = target_weights.find(symbol);
            drift.target_weight = (target_it != target_weights.end()) ? target_it->second : 0.0;

            // Calculate drift
            drift.drift = drift.current_weight - drift.target_weight;

            // Calculate rebalance requirements
            double target_value   = holdings.total_value() * drift.target_weight;
            double current_value  = holdings.total_value() * drift.current_weight;
            drift.rebalance_value = target_value - current_value;

            if (holding_result.is_ok() && holding_result.value().current_price > 0) {
                drift.rebalance_shares = drift.rebalance_value / holding_result.value().current_price;
            } else {
                drift.rebalance_shares = 0.0;
            }

            drifts.push_back(drift);
        }

        // Sort by absolute drift
        std::sort(drifts.begin(), drifts.end(), [](const AllocationDrift& a, const AllocationDrift& b) {
            return std::abs(a.drift) > std::abs(b.drift);
        });

        return Result<std::vector<AllocationDrift>>::success(std::move(drifts));
    }

    /**
     * @brief Analyze allocation stability over time
     */
    Result<DataFrame> analyze_allocation_stability(const HoldingsSeries& holdings_series) const {
        if (holdings_series.size() < 2) {
            return Result<DataFrame>::error(ErrorCode::InsufficientData,
                                            "Need at least 2 holdings snapshots for stability analysis");
        }

        std::vector<DateTime> timestamps;
        std::vector<double> allocation_changes;
        std::vector<double> turnover_rates;
        std::vector<int> position_changes;

        for (size_t i = 1; i < holdings_series.size(); ++i) {
            const auto& prev = holdings_series[i - 1];
            const auto& curr = holdings_series[i];

            timestamps.push_back(curr.timestamp());

            // Calculate allocation change (sum of absolute weight changes)
            double total_change = 0.0;
            std::set<Symbol> all_symbols;

            for (const auto& [symbol, holding] : prev.holdings()) {
                all_symbols.insert(symbol);
            }
            for (const auto& [symbol, holding] : curr.holdings()) {
                all_symbols.insert(symbol);
            }

            for (const auto& symbol : all_symbols) {
                double prev_weight = 0.0;
                double curr_weight = 0.0;

                auto prev_holding = prev.get_holding(symbol);
                if (prev_holding.is_ok()) {
                    prev_weight = prev_holding.value().weight;
                }

                auto curr_holding = curr.get_holding(symbol);
                if (curr_holding.is_ok()) {
                    curr_weight = curr_holding.value().weight;
                }

                total_change += std::abs(curr_weight - prev_weight);
            }

            allocation_changes.push_back(total_change);

            // Turnover rate (half of total change to avoid double counting)
            turnover_rates.push_back(total_change / 2.0);

            // Position changes
            int positions_added   = 0;
            int positions_removed = 0;

            for (const auto& [symbol, holding] : curr.holdings()) {
                if (prev.get_holding(symbol).is_error()) {
                    positions_added++;
                }
            }

            for (const auto& [symbol, holding] : prev.holdings()) {
                if (curr.get_holding(symbol).is_error()) {
                    positions_removed++;
                }
            }

            position_changes.push_back(positions_added + positions_removed);
        }

        DataFrame df{timestamps};
        df.add_column("allocation_change", allocation_changes);
        df.add_column("turnover_rate", turnover_rates);
        df.add_column("position_changes", position_changes);

        return Result<DataFrame>::success(std::move(df));
    }

  private:
    double calculate_gini_coefficient(const std::vector<double>& weights) const {
        if (weights.empty())
            return 0.0;

        std::vector<double> sorted_weights = weights;
        std::sort(sorted_weights.begin(), sorted_weights.end());

        double sum_of_absolute_differences = 0.0;
        double sum_of_weights              = 0.0;

        for (size_t i = 0; i < sorted_weights.size(); ++i) {
            sum_of_weights += sorted_weights[i];
            sum_of_absolute_differences += (2.0 * (i + 1) - sorted_weights.size() - 1) * sorted_weights[i];
        }

        if (sum_of_weights == 0.0)
            return 0.0;

        return sum_of_absolute_differences / (sorted_weights.size() * sum_of_weights);
    }
};

/**
 * @brief Market cap bucket for classification
 */
enum class MarketCapBucket {
    MegaCap,   // > $200B
    LargeCap,  // $10B - $200B
    MidCap,    // $2B - $10B
    SmallCap,  // $300M - $2B
    MicroCap,  // < $300M
    Unknown
};

/**
 * @brief Classify market cap bucket
 */
inline MarketCapBucket classify_market_cap(double market_cap_millions) {
    if (market_cap_millions > 200000)
        return MarketCapBucket::MegaCap;
    if (market_cap_millions > 10000)
        return MarketCapBucket::LargeCap;
    if (market_cap_millions > 2000)
        return MarketCapBucket::MidCap;
    if (market_cap_millions > 300)
        return MarketCapBucket::SmallCap;
    if (market_cap_millions > 0)
        return MarketCapBucket::MicroCap;
    return MarketCapBucket::Unknown;
}

/**
 * @brief Convert market cap bucket to string
 */
inline std::string market_cap_bucket_to_string(MarketCapBucket bucket) {
    switch (bucket) {
        case MarketCapBucket::MegaCap:
            return "Mega Cap";
        case MarketCapBucket::LargeCap:
            return "Large Cap";
        case MarketCapBucket::MidCap:
            return "Mid Cap";
        case MarketCapBucket::SmallCap:
            return "Small Cap";
        case MarketCapBucket::MicroCap:
            return "Micro Cap";
        default:
            return "Unknown";
    }
}

}  // namespace pyfolio::positions