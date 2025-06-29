#pragma once

#include "../core/dataframe.h"
#include "../core/datetime.h"
#include "../core/error_handling.h"
#include "../core/types.h"
#include "../math/statistics.h"
#include <algorithm>
#include <map>
#include <numeric>
#include <vector>

namespace pyfolio::risk {

/**
 * @brief Factor exposure data for a single security
 */
struct SecurityFactorExposure {
    Symbol symbol;
    std::map<std::string, double> factor_loadings;

    /**
     * @brief Get exposure to a specific factor
     */
    double get_factor_exposure(const std::string& factor_name) const {
        auto it = factor_loadings.find(factor_name);
        return (it != factor_loadings.end()) ? it->second : 0.0;
    }

    /**
     * @brief Check if security has exposure to factor
     */
    bool has_factor(const std::string& factor_name) const {
        return factor_loadings.find(factor_name) != factor_loadings.end();
    }
};

/**
 * @brief Portfolio-level factor exposure analysis
 */
struct PortfolioFactorExposure {
    std::map<std::string, double> net_exposures;
    std::map<std::string, double> gross_exposures;
    std::map<std::string, double> active_exposures;  // vs benchmark
    std::map<std::string, double> exposure_concentrations;

    /**
     * @brief Get net exposure to a factor
     */
    double get_net_exposure(const std::string& factor_name) const {
        auto it = net_exposures.find(factor_name);
        return (it != net_exposures.end()) ? it->second : 0.0;
    }

    /**
     * @brief Get gross exposure to a factor
     */
    double get_gross_exposure(const std::string& factor_name) const {
        auto it = gross_exposures.find(factor_name);
        return (it != gross_exposures.end()) ? it->second : 0.0;
    }

    /**
     * @brief Get active exposure vs benchmark
     */
    double get_active_exposure(const std::string& factor_name) const {
        auto it = active_exposures.find(factor_name);
        return (it != active_exposures.end()) ? it->second : 0.0;
    }
};

/**
 * @brief Factor risk attribution results
 */
struct FactorRiskAttribution {
    std::map<std::string, double> factor_contributions;
    double specific_risk_contribution;
    double total_risk;

    /**
     * @brief Get contribution of a specific factor to portfolio risk
     */
    double get_factor_contribution(const std::string& factor_name) const {
        auto it = factor_contributions.find(factor_name);
        return (it != factor_contributions.end()) ? it->second : 0.0;
    }

    /**
     * @brief Get percentage contribution of a factor
     */
    double get_factor_contribution_pct(const std::string& factor_name) const {
        if (total_risk == 0.0)
            return 0.0;
        return get_factor_contribution(factor_name) / total_risk;
    }
};

/**
 * @brief Multi-factor model for risk and return analysis
 */
class FactorModel {
  private:
    std::map<std::string, TimeSeries<double>> factor_returns_;
    std::map<Symbol, SecurityFactorExposure> security_exposures_;
    std::map<std::string, std::map<std::string, double>> factor_covariance_;

  public:
    /**
     * @brief Set factor return time series
     */
    void set_factor_returns(const std::map<std::string, TimeSeries<double>>& factor_returns) {
        factor_returns_ = factor_returns;
    }

    /**
     * @brief Set security factor exposures
     */
    void set_security_exposures(const std::map<Symbol, SecurityFactorExposure>& exposures) {
        security_exposures_ = exposures;
    }

    /**
     * @brief Set factor covariance matrix
     */
    void set_factor_covariance(const std::map<std::string, std::map<std::string, double>>& covariance) {
        factor_covariance_ = covariance;
    }

    /**
     * @brief Calculate portfolio factor exposures
     */
    Result<PortfolioFactorExposure> calculate_portfolio_exposures(
        const std::map<Symbol, double>& portfolio_weights,
        const std::map<Symbol, double>& benchmark_weights = {}) const {
        if (portfolio_weights.empty()) {
            return Result<PortfolioFactorExposure>::error(ErrorCode::InvalidInput, "Portfolio weights cannot be empty");
        }

        // Validate weights sum to 1
        double total_weight = std::accumulate(portfolio_weights.begin(), portfolio_weights.end(), 0.0,
                                              [](double sum, const auto& pair) { return sum + std::abs(pair.second); });

        if (std::abs(total_weight - 1.0) > 1e-6) {
            return Result<PortfolioFactorExposure>::error(ErrorCode::InvalidInput, "Portfolio weights must sum to 1.0");
        }

        PortfolioFactorExposure result;

        // Get all unique factors
        std::set<std::string> all_factors;
        for (const auto& [symbol, exposure] : security_exposures_) {
            for (const auto& [factor, loading] : exposure.factor_loadings) {
                all_factors.insert(factor);
            }
        }

        // Calculate portfolio exposures
        for (const std::string& factor : all_factors) {
            double net_exposure       = 0.0;
            double gross_exposure     = 0.0;
            double benchmark_exposure = 0.0;

            // Portfolio exposure
            for (const auto& [symbol, weight] : portfolio_weights) {
                auto exposure_it = security_exposures_.find(symbol);
                if (exposure_it != security_exposures_.end()) {
                    double factor_loading = exposure_it->second.get_factor_exposure(factor);
                    net_exposure += weight * factor_loading;
                    gross_exposure += std::abs(weight * factor_loading);
                }
            }

            // Benchmark exposure (if provided)
            if (!benchmark_weights.empty()) {
                for (const auto& [symbol, weight] : benchmark_weights) {
                    auto exposure_it = security_exposures_.find(symbol);
                    if (exposure_it != security_exposures_.end()) {
                        double factor_loading = exposure_it->second.get_factor_exposure(factor);
                        benchmark_exposure += weight * factor_loading;
                    }
                }
            }

            result.net_exposures[factor]    = net_exposure;
            result.gross_exposures[factor]  = gross_exposure;
            result.active_exposures[factor] = net_exposure - benchmark_exposure;

            // Calculate concentration (sum of squares)
            double concentration = 0.0;
            for (const auto& [symbol, weight] : portfolio_weights) {
                auto exposure_it = security_exposures_.find(symbol);
                if (exposure_it != security_exposures_.end()) {
                    double factor_loading = exposure_it->second.get_factor_exposure(factor);
                    concentration += (weight * factor_loading) * (weight * factor_loading);
                }
            }
            result.exposure_concentrations[factor] = concentration;
        }

        return Result<PortfolioFactorExposure>::success(std::move(result));
    }

    /**
     * @brief Calculate factor risk attribution
     */
    Result<FactorRiskAttribution> calculate_risk_attribution(
        const std::map<Symbol, double>& portfolio_weights, const std::map<Symbol, double>& specific_risks = {}) const {
        // Calculate portfolio factor exposures
        auto exposure_result = calculate_portfolio_exposures(portfolio_weights);
        if (exposure_result.is_error()) {
            return Result<FactorRiskAttribution>::error(exposure_result.error().code, exposure_result.error().message);
        }

        const auto& exposures = exposure_result.value();
        FactorRiskAttribution result;

        // Calculate factor risk contributions
        for (const auto& [factor1, exposure1] : exposures.net_exposures) {
            double factor_contribution = 0.0;

            for (const auto& [factor2, exposure2] : exposures.net_exposures) {
                // Get factor covariance
                double covariance = get_factor_covariance(factor1, factor2);
                factor_contribution += exposure1 * exposure2 * covariance;
            }

            result.factor_contributions[factor1] = factor_contribution;
        }

        // Calculate specific risk contribution
        result.specific_risk_contribution = 0.0;
        for (const auto& [symbol, weight] : portfolio_weights) {
            auto specific_risk_it = specific_risks.find(symbol);
            if (specific_risk_it != specific_risks.end()) {
                double specific_risk = specific_risk_it->second;
                result.specific_risk_contribution += weight * weight * specific_risk * specific_risk;
            }
        }

        // Calculate total risk
        result.total_risk = 0.0;
        for (const auto& [factor, contribution] : result.factor_contributions) {
            result.total_risk += contribution;
        }
        result.total_risk += result.specific_risk_contribution;
        result.total_risk = std::sqrt(std::max(0.0, result.total_risk));

        return Result<FactorRiskAttribution>::success(std::move(result));
    }

    /**
     * @brief Estimate expected returns using factor model
     */
    Result<std::map<Symbol, double>> estimate_expected_returns(
        const std::map<std::string, double>& factor_expected_returns) const {
        if (factor_expected_returns.empty()) {
            return Result<std::map<Symbol, double>>::error(ErrorCode::InvalidInput,
                                                           "Factor expected returns cannot be empty");
        }

        std::map<Symbol, double> expected_returns;

        for (const auto& [symbol, exposure] : security_exposures_) {
            double expected_return = 0.0;

            for (const auto& [factor, loading] : exposure.factor_loadings) {
                auto expected_it = factor_expected_returns.find(factor);
                if (expected_it != factor_expected_returns.end()) {
                    expected_return += loading * expected_it->second;
                }
            }

            expected_returns[symbol] = expected_return;
        }

        return Result<std::map<Symbol, double>>::success(std::move(expected_returns));
    }

    /**
     * @brief Get list of available factors
     */
    std::vector<std::string> get_available_factors() const {
        std::set<std::string> factors;

        for (const auto& [symbol, exposure] : security_exposures_) {
            for (const auto& [factor, loading] : exposure.factor_loadings) {
                factors.insert(factor);
            }
        }

        return std::vector<std::string>(factors.begin(), factors.end());
    }

  private:
    /**
     * @brief Get covariance between two factors
     */
    double get_factor_covariance(const std::string& factor1, const std::string& factor2) const {
        auto it1 = factor_covariance_.find(factor1);
        if (it1 != factor_covariance_.end()) {
            auto it2 = it1->second.find(factor2);
            if (it2 != it1->second.end()) {
                return it2->second;
            }
        }

        // Return zero if covariance not found
        return 0.0;
    }
};

/**
 * @brief Common factor models and utilities
 */
namespace common_factors {

/**
 * @brief Fama-French 3-factor model setup
 */
inline std::vector<std::string> fama_french_3_factors() {
    return {"Market", "SMB", "HML"};
}

/**
 * @brief Fama-French 5-factor model setup
 */
inline std::vector<std::string> fama_french_5_factors() {
    return {"Market", "SMB", "HML", "RMW", "CMA"};
}

/**
 * @brief BARRA-style fundamental factors
 */
inline std::vector<std::string> barra_fundamental_factors() {
    return {"Size", "Value", "Quality", "Momentum", "Volatility", "Growth", "Profitability", "Leverage", "Liquidity"};
}

/**
 * @brief Sector factors (GICS level 1)
 */
inline std::vector<std::string> gics_sector_factors() {
    return {"Technology",    "Healthcare",  "Financial",        "Consumer_Discretionary",
            "Communication", "Industrial",  "Consumer_Staples", "Energy",
            "Utilities",     "Real_Estate", "Materials"};
}

}  // namespace common_factors

/**
 * @brief Create a simple factor model from return data
 */
inline Result<FactorModel> create_factor_model_from_returns(
    const std::map<std::string, TimeSeries<double>>& factor_returns,
    const std::map<Symbol, std::map<std::string, double>>& factor_loadings) {
    if (factor_returns.empty()) {
        return Result<FactorModel>::error(ErrorCode::InvalidInput, "Factor returns cannot be empty");
    }

    FactorModel model;
    model.set_factor_returns(factor_returns);

    // Convert loadings to SecurityFactorExposure format
    std::map<Symbol, SecurityFactorExposure> exposures;
    for (const auto& [symbol, loadings] : factor_loadings) {
        SecurityFactorExposure exposure;
        exposure.symbol          = symbol;
        exposure.factor_loadings = loadings;
        exposures[symbol]        = exposure;
    }
    model.set_security_exposures(exposures);

    // Calculate factor covariance matrix
    std::map<std::string, std::map<std::string, double>> covariance;
    std::vector<std::string> factor_names;
    for (const auto& [factor, ts] : factor_returns) {
        factor_names.push_back(factor);
    }

    for (const auto& factor1 : factor_names) {
        for (const auto& factor2 : factor_names) {
            const auto& ts1 = factor_returns.at(factor1);
            const auto& ts2 = factor_returns.at(factor2);

            if (ts1.size() == ts2.size() && ts1.size() > 1) {
                auto cov_result =
                    stats::covariance(std::span<const double>{ts1.values()}, std::span<const double>{ts2.values()});

                if (cov_result.is_ok()) {
                    covariance[factor1][factor2] = cov_result.value();
                } else {
                    covariance[factor1][factor2] = (factor1 == factor2) ? 1.0 : 0.0;
                }
            } else {
                covariance[factor1][factor2] = (factor1 == factor2) ? 1.0 : 0.0;
            }
        }
    }

    model.set_factor_covariance(covariance);

    return Result<FactorModel>::success(std::move(model));
}

}  // namespace pyfolio::risk