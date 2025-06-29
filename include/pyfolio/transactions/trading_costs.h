#pragma once

#include "../core/dataframe.h"
#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include "transaction.h"
#include <cmath>

namespace pyfolio::transactions {

/**
 * @brief Trading cost components
 */
struct TradingCostBreakdown {
    double commission;
    double slippage;
    double market_impact;
    double opportunity_cost;
    double total_cost;

    TradingCostBreakdown()
        : commission(0.0), slippage(0.0), market_impact(0.0), opportunity_cost(0.0), total_cost(0.0) {}

    void calculate_total() { total_cost = commission + slippage + market_impact + opportunity_cost; }
};

/**
 * @brief Market impact model interface
 */
class MarketImpactModel {
  public:
    virtual ~MarketImpactModel() = default;

    /**
     * @brief Calculate expected market impact
     * @param shares Number of shares traded
     * @param adv Average daily volume
     * @param volatility Stock volatility
     * @param spread Bid-ask spread
     * @return Expected market impact in basis points
     */
    virtual double calculate_impact(Shares shares, Volume adv, double volatility, double spread) const = 0;
};

/**
 * @brief Linear market impact model
 */
class LinearImpactModel : public MarketImpactModel {
  private:
    double impact_coefficient_;

  public:
    explicit LinearImpactModel(double impact_coefficient = 10.0) : impact_coefficient_(impact_coefficient) {}

    double calculate_impact(Shares shares, Volume adv, double volatility, double spread) const override {
        if (adv <= 0)
            return 0.0;

        double participation_rate = std::abs(shares) / adv;
        return impact_coefficient_ * participation_rate * 10000.0;  // basis points
    }
};

/**
 * @brief Square-root market impact model (Almgren et al.)
 */
class SquareRootImpactModel : public MarketImpactModel {
  private:
    double temporary_impact_coefficient_;
    double permanent_impact_coefficient_;

  public:
    SquareRootImpactModel(double temp_coeff = 0.1, double perm_coeff = 0.1)
        : temporary_impact_coefficient_(temp_coeff), permanent_impact_coefficient_(perm_coeff) {}

    double calculate_impact(Shares shares, Volume adv, double volatility, double spread) const override {
        if (adv <= 0)
            return 0.0;

        double participation_rate = std::abs(shares) / adv;
        double sigma_over_v       = volatility / std::pow(adv, 0.5);

        // Temporary impact
        double temp_impact = temporary_impact_coefficient_ * spread * std::sqrt(participation_rate);

        // Permanent impact
        double perm_impact = permanent_impact_coefficient_ * sigma_over_v * participation_rate;

        return (temp_impact + perm_impact) * 10000.0;  // basis points
    }
};

/**
 * @brief Trading cost analyzer
 */
class TradingCostAnalyzer {
  private:
    std::unique_ptr<MarketImpactModel> impact_model_;
    double commission_rate_;
    double default_spread_;

  public:
    TradingCostAnalyzer(std::unique_ptr<MarketImpactModel> impact_model = nullptr,
                        double commission_rate                          = 0.001,  // 10 bps
                        double default_spread                           = 0.0002)                           // 2 bps
        : impact_model_(impact_model ? std::move(impact_model) : std::make_unique<SquareRootImpactModel>()),
          commission_rate_(commission_rate), default_spread_(default_spread) {}

    /**
     * @brief Analyze trading costs for a transaction series
     */
    Result<DataFrame> analyze_costs(const TransactionSeries& transactions,
                                    const std::map<Symbol, MarketData>& market_data) {
        if (transactions.empty()) {
            return Result<DataFrame>::error(ErrorCode::InsufficientData, "No transactions to analyze");
        }

        std::vector<DateTime> timestamps;
        std::vector<std::string> symbols;
        std::vector<double> commissions;
        std::vector<double> slippages;
        std::vector<double> market_impacts;
        std::vector<double> total_costs;

        for (const auto& txn : transactions) {
            timestamps.push_back(txn.timestamp());
            symbols.push_back(txn.symbol());

            // Direct costs
            commissions.push_back(txn.commission());
            double slippage_cost = std::abs(txn.shares() * txn.slippage());
            slippages.push_back(slippage_cost);

            // Market impact estimation
            double impact = 0.0;
            auto it       = market_data.find(txn.symbol());
            if (it != market_data.end()) {
                impact = estimate_market_impact(txn, it->second);
            }
            market_impacts.push_back(impact);

            // Total cost
            total_costs.push_back(txn.commission() + slippage_cost + impact);
        }

        DataFrame df{timestamps};
        df.add_column("symbol", symbols);
        df.add_column("commission", commissions);
        df.add_column("slippage", slippages);
        df.add_column("market_impact", market_impacts);
        df.add_column("total_cost", total_costs);

        return Result<DataFrame>::success(std::move(df));
    }

    /**
     * @brief Calculate implementation shortfall
     */
    Result<double> calculate_implementation_shortfall(const TransactionSeries& transactions,
                                                      const std::map<Symbol, PriceSeries>& benchmark_prices) {
        if (transactions.empty()) {
            return Result<double>::error(ErrorCode::InsufficientData, "No transactions to analyze");
        }

        double total_shortfall = 0.0;

        for (const auto& txn : transactions) {
            auto it = benchmark_prices.find(txn.symbol());
            if (it == benchmark_prices.end()) {
                continue;
            }

            // Find benchmark price at decision time
            auto benchmark_result = it->second.at_time(txn.timestamp());
            if (benchmark_result.is_error()) {
                continue;
            }

            double benchmark_price = benchmark_result.value();
            double execution_price = txn.price();

            // Shortfall = (execution_price - benchmark_price) * shares (for buys)
            // Shortfall = (benchmark_price - execution_price) * shares (for sells)
            double price_diff =
                txn.is_buy() ? (execution_price - benchmark_price) : (benchmark_price - execution_price);

            double shortfall = price_diff * std::abs(txn.shares());
            total_shortfall += shortfall;
        }

        return Result<double>::success(total_shortfall);
    }

    /**
     * @brief Calculate effective spread
     */
    Result<double> calculate_effective_spread(const TransactionRecord& transaction, Price midpoint_price) {
        if (midpoint_price <= 0) {
            return Result<double>::error(ErrorCode::InvalidInput, "Midpoint price must be positive");
        }

        // Effective spread = 2 * |execution_price - midpoint_price| / midpoint_price
        double spread = 2.0 * std::abs(transaction.price() - midpoint_price) / midpoint_price;

        return Result<double>::success(spread);
    }

    /**
     * @brief Calculate cost breakdown by time period
     */
    Result<DataFrame> cost_breakdown_by_period(const TransactionSeries& transactions, Frequency frequency) {
        if (transactions.empty()) {
            return Result<DataFrame>::error(ErrorCode::InsufficientData, "No transactions to analyze");
        }

        // Group transactions by period
        std::map<DateTime, TradingCostBreakdown> period_costs;

        for (const auto& txn : transactions) {
            DateTime period_start = get_period_start(txn.timestamp(), frequency);

            auto& costs = period_costs[period_start];
            costs.commission += txn.commission();
            costs.slippage += std::abs(txn.shares() * txn.slippage());
            // Market impact would need market data
        }

        // Create DataFrame
        std::vector<DateTime> periods;
        std::vector<double> commissions;
        std::vector<double> slippages;
        std::vector<double> total_costs;

        for (auto& [period, costs] : period_costs) {
            costs.calculate_total();
            periods.push_back(period);
            commissions.push_back(costs.commission);
            slippages.push_back(costs.slippage);
            total_costs.push_back(costs.total_cost);
        }

        DataFrame df{periods};
        df.add_column("commission", commissions);
        df.add_column("slippage", slippages);
        df.add_column("total_cost", total_costs);

        return Result<DataFrame>::success(std::move(df));
    }

    /**
     * @brief Estimate optimal trade size given cost constraints
     */
    Result<Shares> estimate_optimal_trade_size(const Symbol& symbol, const MarketData& market_data,
                                               double max_cost_bps = 50.0) {
        if (market_data.volumes.empty() || market_data.prices.empty()) {
            return Result<Shares>::error(ErrorCode::InsufficientData, "Insufficient market data");
        }

        // Calculate average daily volume
        double avg_volume =
            std::accumulate(market_data.volumes.begin(), market_data.volumes.end(), 0.0) / market_data.volumes.size();

        if (avg_volume <= 0) {
            return Result<Shares>::error(ErrorCode::InvalidInput, "Average volume must be positive");
        }

        // Binary search for optimal size
        double low       = 0.0;
        double high      = avg_volume * 0.2;  // Max 20% of ADV
        double tolerance = 1.0;

        while (high - low > tolerance) {
            double mid = (low + high) / 2.0;

            // Estimate cost for this size
            double volatility = estimate_volatility(market_data.prices);
            double impact_bps = impact_model_->calculate_impact(mid, avg_volume, volatility, default_spread_);

            double total_cost_bps = commission_rate_ * 10000.0 + impact_bps;

            if (total_cost_bps <= max_cost_bps) {
                low = mid;
            } else {
                high = mid;
            }
        }

        return Result<Shares>::success(low);
    }

  private:
    double estimate_market_impact(const TransactionRecord& txn, const MarketData& market_data) {
        if (market_data.volumes.empty() || market_data.prices.empty()) {
            return 0.0;
        }

        // Calculate average daily volume
        double avg_volume =
            std::accumulate(market_data.volumes.begin(), market_data.volumes.end(), 0.0) / market_data.volumes.size();

        // Estimate volatility
        double volatility = estimate_volatility(market_data.prices);

        // Calculate impact in basis points
        double impact_bps = impact_model_->calculate_impact(txn.shares(), avg_volume, volatility, default_spread_);

        // Convert to dollar impact
        return std::abs(txn.shares() * txn.price() * impact_bps / 10000.0);
    }

    double estimate_volatility(const std::vector<Price>& prices) {
        if (prices.size() < 2)
            return 0.02;  // Default 2% volatility

        std::vector<double> returns;
        for (size_t i = 1; i < prices.size(); ++i) {
            returns.push_back(std::log(prices[i] / prices[i - 1]));
        }

        double mean     = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        double variance = 0.0;

        for (double ret : returns) {
            variance += (ret - mean) * (ret - mean);
        }

        variance /= (returns.size() - 1);
        return std::sqrt(variance * constants::TRADING_DAYS_PER_YEAR);
    }

    DateTime get_period_start(const DateTime& timestamp, Frequency freq) {
        auto date = timestamp.to_date();

        switch (freq) {
            case Frequency::Monthly:
                return DateTime{date.year() / date.month() / std::chrono::day{1}};
            case Frequency::Quarterly: {
                auto month               = date.month();
                auto quarter_start_month = ((static_cast<unsigned>(month) - 1) / 3) * 3 + 1;
                return DateTime{date.year() / std::chrono::month{quarter_start_month} / std::chrono::day{1}};
            }
            case Frequency::Yearly:
                return DateTime{date.year() / std::chrono::January / std::chrono::day{1}};
            default:
                return timestamp;
        }
    }
};

/**
 * @brief Calculate total trading costs as percentage of portfolio value
 */
inline Result<double> calculate_cost_ratio(const TransactionSeries& transactions, double portfolio_value) {
    if (portfolio_value <= 0) {
        return Result<double>::error(ErrorCode::InvalidInput, "Portfolio value must be positive");
    }

    double total_costs = transactions.total_commissions() + transactions.total_slippage();
    double cost_ratio  = total_costs / portfolio_value;

    return Result<double>::success(cost_ratio);
}

/**
 * @brief Analyze trading costs by symbol
 */
inline Result<std::map<Symbol, TradingCostBreakdown>> analyze_costs_by_symbol(const TransactionSeries& transactions) {
    auto grouped = transactions.group_by_symbol();
    std::map<Symbol, TradingCostBreakdown> symbol_costs;

    for (const auto& [symbol, symbol_txns] : grouped) {
        TradingCostBreakdown costs;
        costs.commission = symbol_txns.total_commissions();
        costs.slippage   = symbol_txns.total_slippage();
        costs.calculate_total();

        symbol_costs[symbol] = costs;
    }

    return Result<std::map<Symbol, TradingCostBreakdown>>::success(symbol_costs);
}

}  // namespace pyfolio::transactions