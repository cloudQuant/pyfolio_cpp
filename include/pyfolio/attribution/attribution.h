#pragma once

#include "../core/dataframe.h"
#include "../core/datetime.h"
#include "../core/error_handling.h"
#include "../core/types.h"
#include "../math/statistics.h"
#include "../performance/returns.h"
#include "../positions/holdings.h"
#include <map>
#include <vector>

namespace pyfolio::attribution {

/**
 * @brief Attribution result for a single period
 */
struct AttributionResult {
    double portfolio_return;
    double benchmark_return;
    double active_return;
    double allocation_effect;
    double selection_effect;
    double interaction_effect;
    double total_effect;

    /**
     * @brief Validate attribution consistency
     */
    bool is_consistent(double tolerance = 1e-6) const {
        double calculated_total = allocation_effect + selection_effect + interaction_effect;
        return std::abs(calculated_total - total_effect) < tolerance &&
               std::abs(active_return - total_effect) < tolerance;
    }
};

/**
 * @brief Sector-level attribution data
 */
struct SectorAttribution {
    std::string sector;
    double portfolio_weight;
    double benchmark_weight;
    double portfolio_return;
    double benchmark_return;
    double allocation_effect;
    double selection_effect;
    double interaction_effect;
    double total_contribution;
};

/**
 * @brief Factor exposures structure
 */
struct FactorExposures {
    double market_beta;
    double size_factor;
    double value_factor;
    double momentum_factor;
    double quality_factor;
    double low_volatility_factor;

    FactorExposures() = default;
    FactorExposures(double mb, double sf, double vf, double mf, double qf, double lvf)
        : market_beta(mb), size_factor(sf), value_factor(vf), momentum_factor(mf), quality_factor(qf),
          low_volatility_factor(lvf) {}
};

/**
 * @brief Factor returns structure
 */
struct FactorReturns {
    double market_return;
    double size_return;
    double value_return;
    double momentum_return;
    double quality_return;
    double low_volatility_return;

    FactorReturns() = default;
    FactorReturns(double mr, double sr, double vr, double mor, double qr, double lvr)
        : market_return(mr), size_return(sr), value_return(vr), momentum_return(mor), quality_return(qr),
          low_volatility_return(lvr) {}
};

/**
 * @brief Brinson-Hood-Beebower attribution model
 */
class BrinsonAttribution {
  private:
    std::map<Symbol, std::string> symbol_to_sector_;

  public:
    /**
     * @brief Set sector mapping for securities
     */
    void set_sector_mapping(const std::map<Symbol, std::string>& mapping) { symbol_to_sector_ = mapping; }

    /**
     * @brief Calculate attribution for a single period
     */
    Result<AttributionResult> calculate_period_attribution(const positions::PortfolioHoldings& portfolio_start,
                                                           const positions::PortfolioHoldings& portfolio_end,
                                                           const std::map<Symbol, double>& benchmark_weights,
                                                           const std::map<Symbol, double>& security_returns) const {
        if (portfolio_start.holdings().empty() || portfolio_end.holdings().empty()) {
            return Result<AttributionResult>::error(ErrorCode::InsufficientData, "Portfolio holdings cannot be empty");
        }

        AttributionResult result{};

        // Calculate portfolio return
        double portfolio_start_value = portfolio_start.total_value();
        double portfolio_end_value   = portfolio_end.total_value();

        if (portfolio_start_value <= 0) {
            return Result<AttributionResult>::error(ErrorCode::InvalidInput, "Portfolio start value must be positive");
        }

        result.portfolio_return = (portfolio_end_value - portfolio_start_value) / portfolio_start_value;

        // Calculate benchmark return
        result.benchmark_return       = 0.0;
        double total_benchmark_weight = 0.0;

        for (const auto& [symbol, weight] : benchmark_weights) {
            auto return_it = security_returns.find(symbol);
            if (return_it != security_returns.end()) {
                result.benchmark_return += weight * return_it->second;
                total_benchmark_weight += weight;
            }
        }

        if (total_benchmark_weight == 0.0) {
            return Result<AttributionResult>::error(ErrorCode::InvalidInput, "Benchmark weights sum to zero");
        }

        result.benchmark_return /= total_benchmark_weight;
        result.active_return = result.portfolio_return - result.benchmark_return;

        // Calculate attribution effects
        auto attribution_effects = calculate_attribution_effects(portfolio_start, benchmark_weights, security_returns);

        if (attribution_effects.is_error()) {
            return Result<AttributionResult>::error(attribution_effects.error().code,
                                                    attribution_effects.error().message);
        }

        const auto& effects       = attribution_effects.value();
        result.allocation_effect  = effects.allocation_effect;
        result.selection_effect   = effects.selection_effect;
        result.interaction_effect = effects.interaction_effect;
        result.total_effect       = result.allocation_effect + result.selection_effect + result.interaction_effect;

        return Result<AttributionResult>::success(result);
    }

    /**
     * @brief Calculate sector-level attribution
     */
    Result<std::vector<SectorAttribution>> calculate_sector_attribution(
        const positions::PortfolioHoldings& portfolio, const std::map<Symbol, double>& benchmark_weights,
        const std::map<Symbol, double>& security_returns) const {
        // Aggregate portfolio and benchmark by sector
        std::map<std::string, double> portfolio_sector_weights;
        std::map<std::string, double> benchmark_sector_weights;
        std::map<std::string, double> sector_returns;
        std::map<std::string, std::vector<Symbol>> sector_symbols;

        // Portfolio sector weights
        for (const auto& [symbol, holding] : portfolio.holdings()) {
            std::string sector = get_symbol_sector(symbol);
            portfolio_sector_weights[sector] += holding.weight;
            sector_symbols[sector].push_back(symbol);
        }

        // Benchmark sector weights and returns
        for (const auto& [symbol, weight] : benchmark_weights) {
            std::string sector = get_symbol_sector(symbol);
            benchmark_sector_weights[sector] += weight;

            auto return_it = security_returns.find(symbol);
            if (return_it != security_returns.end()) {
                // Weight by benchmark weight within sector
                sector_returns[sector] += weight * return_it->second;
            }
        }

        // Normalize sector returns by sector benchmark weights
        for (auto& [sector, ret] : sector_returns) {
            if (benchmark_sector_weights[sector] > 0) {
                ret /= benchmark_sector_weights[sector];
            }
        }

        // Calculate portfolio sector returns
        std::map<std::string, double> portfolio_sector_returns;
        for (const auto& [sector, symbols] : sector_symbols) {
            double sector_return = 0.0;
            double sector_weight = 0.0;

            for (const auto& symbol : symbols) {
                auto holding_result = portfolio.get_holding(symbol);
                auto return_it      = security_returns.find(symbol);

                if (holding_result.is_ok() && return_it != security_returns.end()) {
                    double weight = holding_result.value().weight;
                    sector_return += weight * return_it->second;
                    sector_weight += weight;
                }
            }

            if (sector_weight > 0) {
                portfolio_sector_returns[sector] = sector_return / sector_weight;
            }
        }

        // Calculate attribution for each sector
        std::vector<SectorAttribution> results;
        std::set<std::string> all_sectors;

        for (const auto& [sector, weight] : portfolio_sector_weights) {
            all_sectors.insert(sector);
        }
        for (const auto& [sector, weight] : benchmark_sector_weights) {
            all_sectors.insert(sector);
        }

        for (const std::string& sector : all_sectors) {
            SectorAttribution attr;
            attr.sector           = sector;
            attr.portfolio_weight = portfolio_sector_weights[sector];
            attr.benchmark_weight = benchmark_sector_weights[sector];
            attr.portfolio_return = portfolio_sector_returns[sector];
            attr.benchmark_return = sector_returns[sector];

            // Brinson attribution formulas
            double weight_diff = attr.portfolio_weight - attr.benchmark_weight;
            double return_diff = attr.portfolio_return - attr.benchmark_return;

            attr.allocation_effect  = weight_diff * attr.benchmark_return;
            attr.selection_effect   = attr.benchmark_weight * return_diff;
            attr.interaction_effect = weight_diff * return_diff;
            attr.total_contribution = attr.allocation_effect + attr.selection_effect + attr.interaction_effect;

            results.push_back(attr);
        }

        // Sort by absolute total contribution
        std::sort(results.begin(), results.end(), [](const SectorAttribution& a, const SectorAttribution& b) {
            return std::abs(a.total_contribution) > std::abs(b.total_contribution);
        });

        return Result<std::vector<SectorAttribution>>::success(std::move(results));
    }

    /**
     * @brief Calculate multi-period attribution
     */
    Result<DataFrame> calculate_multi_period_attribution(
        const positions::HoldingsSeries& holdings_series,
        const std::map<DateTime, std::map<Symbol, double>>& benchmark_weights_series,
        const std::map<Symbol, TimeSeries<double>>& return_series) const {
        if (holdings_series.size() < 2) {
            return Result<DataFrame>::error(ErrorCode::InsufficientData, "Need at least 2 holdings snapshots");
        }

        std::vector<DateTime> dates;
        std::vector<double> portfolio_returns;
        std::vector<double> benchmark_returns;
        std::vector<double> active_returns;
        std::vector<double> allocation_effects;
        std::vector<double> selection_effects;
        std::vector<double> interaction_effects;

        for (size_t i = 1; i < holdings_series.size(); ++i) {
            const auto& start_holdings = holdings_series[i - 1];
            const auto& end_holdings   = holdings_series[i];

            DateTime period_end = end_holdings.timestamp();
            dates.push_back(period_end);

            // Get benchmark weights for this period
            auto bench_it = benchmark_weights_series.find(period_end);
            if (bench_it == benchmark_weights_series.end()) {
                continue;  // Skip period if no benchmark data
            }

            // Calculate security returns for this period
            std::map<Symbol, double> period_returns;
            DateTime period_start = start_holdings.timestamp();

            for (const auto& [symbol, ts] : return_series) {
                auto start_result = ts.at_time(period_start);
                auto end_result   = ts.at_time(period_end);

                if (start_result.is_ok() && end_result.is_ok()) {
                    double start_price = start_result.value();
                    double end_price   = end_result.value();
                    if (start_price > 0) {
                        period_returns[symbol] = (end_price - start_price) / start_price;
                    }
                }
            }

            // Calculate attribution for this period
            auto attribution_result =
                calculate_period_attribution(start_holdings, end_holdings, bench_it->second, period_returns);

            if (attribution_result.is_ok()) {
                const auto& attr = attribution_result.value();
                portfolio_returns.push_back(attr.portfolio_return);
                benchmark_returns.push_back(attr.benchmark_return);
                active_returns.push_back(attr.active_return);
                allocation_effects.push_back(attr.allocation_effect);
                selection_effects.push_back(attr.selection_effect);
                interaction_effects.push_back(attr.interaction_effect);
            } else {
                // Fill with zeros if calculation fails
                portfolio_returns.push_back(0.0);
                benchmark_returns.push_back(0.0);
                active_returns.push_back(0.0);
                allocation_effects.push_back(0.0);
                selection_effects.push_back(0.0);
                interaction_effects.push_back(0.0);
            }
        }

        DataFrame df{dates};
        df.add_column("portfolio_return", portfolio_returns);
        df.add_column("benchmark_return", benchmark_returns);
        df.add_column("active_return", active_returns);
        df.add_column("allocation_effect", allocation_effects);
        df.add_column("selection_effect", selection_effects);
        df.add_column("interaction_effect", interaction_effects);

        return Result<DataFrame>::success(std::move(df));
    }

  private:
    std::string get_symbol_sector(const Symbol& symbol) const {
        auto it = symbol_to_sector_.find(symbol);
        return (it != symbol_to_sector_.end()) ? it->second : "Unknown";
    }

    struct AttributionEffects {
        double allocation_effect;
        double selection_effect;
        double interaction_effect;
    };

    Result<AttributionEffects> calculate_attribution_effects(const positions::PortfolioHoldings& portfolio,
                                                             const std::map<Symbol, double>& benchmark_weights,
                                                             const std::map<Symbol, double>& security_returns) const {
        AttributionEffects effects{0.0, 0.0, 0.0};

        std::set<Symbol> all_symbols;
        for (const auto& [symbol, holding] : portfolio.holdings()) {
            all_symbols.insert(symbol);
        }
        for (const auto& [symbol, weight] : benchmark_weights) {
            all_symbols.insert(symbol);
        }

        for (const Symbol& symbol : all_symbols) {
            double portfolio_weight = 0.0;
            auto holding_result     = portfolio.get_holding(symbol);
            if (holding_result.is_ok()) {
                portfolio_weight = holding_result.value().weight;
            }

            double benchmark_weight = 0.0;
            auto bench_it           = benchmark_weights.find(symbol);
            if (bench_it != benchmark_weights.end()) {
                benchmark_weight = bench_it->second;
            }

            double security_return = 0.0;
            auto return_it         = security_returns.find(symbol);
            if (return_it != security_returns.end()) {
                security_return = return_it->second;
            }

            // Brinson attribution formulas
            double weight_diff = portfolio_weight - benchmark_weight;

            effects.allocation_effect += weight_diff * security_return;
            effects.selection_effect +=
                benchmark_weight * security_return * (portfolio_weight > 0 ? 1.0 : 0.0);  // Simplified
            effects.interaction_effect +=
                weight_diff * security_return * (portfolio_weight > 0 ? 1.0 : 0.0);  // Simplified
        }

        return Result<AttributionEffects>::success(effects);
    }
};

/**
 * @brief Alpha/Beta decomposition analysis
 */
class AlphaBetaAnalysis {
  public:
    struct AlphaBetaResult {
        double alpha;
        double beta;
        double r_squared;
        double tracking_error;
        double information_ratio;
        double active_return;
        double systematic_risk;
        double specific_risk;
    };

    /**
     * @brief Calculate alpha and beta vs benchmark
     */
    Result<AlphaBetaResult> calculate(const ReturnSeries& portfolio_returns, const ReturnSeries& benchmark_returns,
                                      double risk_free_rate = constants::DEFAULT_RISK_FREE_RATE) const {
        if (portfolio_returns.size() != benchmark_returns.size()) {
            return Result<AlphaBetaResult>::error(ErrorCode::InvalidInput,
                                                  "Portfolio and benchmark returns must have same length");
        }

        if (portfolio_returns.size() < 3) {
            return Result<AlphaBetaResult>::error(ErrorCode::InsufficientData,
                                                  "Need at least 3 observations for regression");
        }

        // Convert to excess returns
        auto portfolio_excess_result =
            pyfolio::performance::calculate_excess_returns(portfolio_returns, risk_free_rate);
        auto benchmark_excess_result =
            pyfolio::performance::calculate_excess_returns(benchmark_returns, risk_free_rate);

        if (portfolio_excess_result.is_error() || benchmark_excess_result.is_error()) {
            return Result<AlphaBetaResult>::error(ErrorCode::CalculationError, "Failed to calculate excess returns");
        }

        const auto& port_excess  = portfolio_excess_result.value();
        const auto& bench_excess = benchmark_excess_result.value();

        // Linear regression: portfolio_excess = alpha + beta * benchmark_excess
        auto regression_result = linear_regression(bench_excess.values(), port_excess.values());
        if (regression_result.is_error()) {
            return regression_result;
        }

        AlphaBetaResult result = regression_result.value();

        // Calculate additional metrics
        std::vector<double> active_returns;
        for (size_t i = 0; i < portfolio_returns.size(); ++i) {
            active_returns.push_back(portfolio_returns[i] - benchmark_returns[i]);
        }

        auto mean_active_result = pyfolio::stats::mean(std::span<const double>{active_returns});
        auto std_active_result  = pyfolio::stats::standard_deviation(std::span<const double>{active_returns});

        if (mean_active_result.is_ok() && std_active_result.is_ok()) {
            result.active_return  = mean_active_result.value();
            result.tracking_error = std_active_result.value();

            if (result.tracking_error > 0) {
                result.information_ratio = result.active_return / result.tracking_error;
            }
        }

        // Risk decomposition
        auto bench_vol_result = pyfolio::stats::standard_deviation(std::span<const Return>{benchmark_returns.values()});
        if (bench_vol_result.is_ok()) {
            double benchmark_vol   = bench_vol_result.value();
            result.systematic_risk = std::abs(result.beta) * benchmark_vol;

            auto port_vol_result =
                pyfolio::stats::standard_deviation(std::span<const Return>{portfolio_returns.values()});
            if (port_vol_result.is_ok()) {
                double portfolio_vol = port_vol_result.value();
                result.specific_risk = std::sqrt(
                    std::max(0.0, portfolio_vol * portfolio_vol - result.systematic_risk * result.systematic_risk));
            }
        }

        return Result<AlphaBetaResult>::success(result);
    }

  private:
    Result<AlphaBetaResult> linear_regression(const std::vector<Return>& x, const std::vector<Return>& y) const {
        if (x.size() != y.size() || x.size() < 2) {
            return Result<AlphaBetaResult>::error(ErrorCode::InvalidInput, "Invalid data for regression");
        }

        size_t n      = x.size();
        double sum_x  = std::accumulate(x.begin(), x.end(), 0.0);
        double sum_y  = std::accumulate(y.begin(), y.end(), 0.0);
        double sum_xx = std::inner_product(x.begin(), x.end(), x.begin(), 0.0);
        double sum_xy = std::inner_product(x.begin(), x.end(), y.begin(), 0.0);
        double sum_yy = std::inner_product(y.begin(), y.end(), y.begin(), 0.0);

        double mean_x = sum_x / n;
        double mean_y = sum_y / n;

        double sxx = sum_xx - n * mean_x * mean_x;
        double sxy = sum_xy - n * mean_x * mean_y;
        double syy = sum_yy - n * mean_y * mean_y;

        if (sxx == 0.0) {
            return Result<AlphaBetaResult>::error(ErrorCode::DivisionByZero, "Zero variance in benchmark returns");
        }

        AlphaBetaResult result{};
        result.beta  = sxy / sxx;
        result.alpha = mean_y - result.beta * mean_x;

        // Calculate R-squared
        if (syy > 0.0) {
            result.r_squared = (sxy * sxy) / (sxx * syy);
        }

        return Result<AlphaBetaResult>::success(result);
    }
};

/**
 * @brief General attribution analyzer
 */
class AttributionAnalyzer {
  public:
    /**
     * @brief Analyze factor-based attribution
     */
    Result<double> analyze_factor_attribution(const FactorExposures& portfolio_exposures,
                                              const FactorExposures& benchmark_exposures,
                                              const FactorReturns& factor_returns) {
        double portfolio_return = portfolio_exposures.market_beta * factor_returns.market_return +
                                  portfolio_exposures.size_factor * factor_returns.size_return +
                                  portfolio_exposures.value_factor * factor_returns.value_return +
                                  portfolio_exposures.momentum_factor * factor_returns.momentum_return +
                                  portfolio_exposures.quality_factor * factor_returns.quality_return +
                                  portfolio_exposures.low_volatility_factor * factor_returns.low_volatility_return;

        double benchmark_return = benchmark_exposures.market_beta * factor_returns.market_return +
                                  benchmark_exposures.size_factor * factor_returns.size_return +
                                  benchmark_exposures.value_factor * factor_returns.value_return +
                                  benchmark_exposures.momentum_factor * factor_returns.momentum_return +
                                  benchmark_exposures.quality_factor * factor_returns.quality_return +
                                  benchmark_exposures.low_volatility_factor * factor_returns.low_volatility_return;

        return Result<double>::success(portfolio_return - benchmark_return);
    }

    /**
     * @brief Analyze sector-based attribution
     */
    Result<std::vector<SectorAttribution>> analyze_sector_attribution(
        const std::map<std::string, double>& portfolio_weights, const std::map<std::string, double>& benchmark_weights,
        const std::map<std::string, double>& sector_returns) {
        std::vector<SectorAttribution> results;
        std::set<std::string> all_sectors;

        // Collect all sectors
        for (const auto& [sector, weight] : portfolio_weights)
            all_sectors.insert(sector);
        for (const auto& [sector, weight] : benchmark_weights)
            all_sectors.insert(sector);

        for (const auto& sector : all_sectors) {
            SectorAttribution attr;
            attr.sector = sector;

            // Get weights (default to 0 if not present)
            auto port_it  = portfolio_weights.find(sector);
            auto bench_it = benchmark_weights.find(sector);
            auto ret_it   = sector_returns.find(sector);

            attr.portfolio_weight = (port_it != portfolio_weights.end()) ? port_it->second : 0.0;
            attr.benchmark_weight = (bench_it != benchmark_weights.end()) ? bench_it->second : 0.0;
            attr.portfolio_return = (ret_it != sector_returns.end()) ? ret_it->second : 0.0;
            attr.benchmark_return = attr.portfolio_return;  // Simplified assumption

            // Brinson attribution
            attr.allocation_effect = (attr.portfolio_weight - attr.benchmark_weight) * attr.benchmark_return;
            attr.selection_effect  = attr.benchmark_weight * (attr.portfolio_return - attr.benchmark_return);
            attr.interaction_effect =
                (attr.portfolio_weight - attr.benchmark_weight) * (attr.portfolio_return - attr.benchmark_return);
            attr.total_contribution = attr.allocation_effect + attr.selection_effect + attr.interaction_effect;

            results.push_back(attr);
        }

        return Result<std::vector<SectorAttribution>>::success(results);
    }
};

}  // namespace pyfolio::attribution