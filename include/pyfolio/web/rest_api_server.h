#pragma once

/**
 * @file rest_api_server.h
 * @brief REST API server for pyfolio analytics
 * @version 1.0.0
 * @date 2024
 *
 * This file provides a REST API server using httplib (header-only HTTP library)
 * for pyfolio analytics functionality.
 */

#include "../analytics/parallel_performance_suite.h"
#include "../analytics/performance_analysis_suite.h"
#include "../positions/positions.h"
#include "../transactions/transaction.h"
#include "json_serializer.h"
// data_loader.h not available - removed include
#include <httplib.h>
#include <memory>
#include <string>
#include <thread>

namespace pyfolio::web {

/**
 * @brief REST API configuration
 */
struct ApiConfig {
    std::string host        = "localhost";
    int port                = 8080;
    std::string base_path   = "/api/v1";
    size_t thread_pool_size = std::thread::hardware_concurrency();
    bool enable_cors        = true;
    bool enable_logging     = true;
    std::chrono::seconds request_timeout{30};
};

/**
 * @brief REST API server for pyfolio analytics
 */
class RestApiServer {
  private:
    httplib::Server server_;
    ApiConfig config_;
    std::unique_ptr<analytics::ParallelPerformanceAnalysisSuite> analysis_suite_;
    // Transaction analysis is done directly using TransactionSeries

    /**
     * @brief Setup CORS headers
     */
    void setup_cors() {
        if (config_.enable_cors) {
            server_.set_post_routing_handler([](const httplib::Request& req, httplib::Response& res) {
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
                res.set_header("Access-Control-Allow-Headers", "Content-Type");
            });

            server_.Options(".*", [](const httplib::Request&, httplib::Response& res) {
                res.set_header("Access-Control-Allow-Origin", "*");
                res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
                res.set_header("Access-Control-Allow-Headers", "Content-Type");
                res.status = 200;
            });
        }
    }

    /**
     * @brief Setup logging
     */
    void setup_logging() {
        if (config_.enable_logging) {
            server_.set_logger([](const httplib::Request& req, const httplib::Response& res) {
                std::cout << "[" << DateTime::now().to_iso_string() << "] " << req.method << " " << req.path << " -> "
                          << res.status << std::endl;
            });
        }
    }

    /**
     * @brief Setup error handler
     */
    void setup_error_handler() {
        server_.set_error_handler([](const httplib::Request&, httplib::Response& res) {
            json error_response =
                JsonSerializer::create_error_response(ErrorCode::ServerError, "Internal server error");
            res.set_content(error_response.dump(), "application/json");
        });
    }

  public:
    explicit RestApiServer(const ApiConfig& config = {})
        : config_(config), analysis_suite_(std::make_unique<analytics::ParallelPerformanceAnalysisSuite>()) {
        setup_cors();
        setup_logging();
        setup_error_handler();
        setup_routes();
    }

    /**
     * @brief Setup API routes
     */
    void setup_routes() {
        // Health check endpoint
        server_.Get(config_.base_path + "/health", [](const httplib::Request&, httplib::Response& res) {
            json response = JsonSerializer::create_api_response(true, {
                                                                          { "status", "healthy"},
                                                                          {"version",   "1.0.0"}
            });
            res.set_content(response.dump(), "application/json");
        });

        // Performance analysis endpoint
        server_.Post(
            config_.base_path + "/analyze/performance",
            [this](const httplib::Request& req, httplib::Response& res) { handle_performance_analysis(req, res); });

        // Portfolio metrics endpoint
        server_.Post(
            config_.base_path + "/analyze/portfolio",
            [this](const httplib::Request& req, httplib::Response& res) { handle_portfolio_analysis(req, res); });

        // Transaction analysis endpoint
        server_.Post(
            config_.base_path + "/analyze/transactions",
            [this](const httplib::Request& req, httplib::Response& res) { handle_transaction_analysis(req, res); });

        // Time series calculation endpoints
        server_.Post(
            config_.base_path + "/calculate/sharpe",
            [this](const httplib::Request& req, httplib::Response& res) { handle_sharpe_calculation(req, res); });

        server_.Post(
            config_.base_path + "/calculate/drawdown",
            [this](const httplib::Request& req, httplib::Response& res) { handle_drawdown_calculation(req, res); });

        server_.Post(
            config_.base_path + "/calculate/volatility",
            [this](const httplib::Request& req, httplib::Response& res) { handle_volatility_calculation(req, res); });

        // Batch analysis endpoint
        server_.Post(config_.base_path + "/analyze/batch",
                     [this](const httplib::Request& req, httplib::Response& res) { handle_batch_analysis(req, res); });
    }

    /**
     * @brief Handle performance analysis request
     */
    void handle_performance_analysis(const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);

            // Parse returns time series
            auto returns_result = JsonSerializer::parse_time_series<double>(request_data["returns"]);
            if (returns_result.is_error()) {
                res.status = 400;
                res.set_content(
                    JsonSerializer::create_error_response(returns_result.error().code, returns_result.error().message)
                        .dump(),
                    "application/json");
                return;
            }

            // Parse optional benchmark
            std::optional<TimeSeries<double>> benchmark;
            if (request_data.contains("benchmark")) {
                auto bench_result = JsonSerializer::parse_time_series<double>(request_data["benchmark"]);
                if (bench_result.is_ok()) {
                    benchmark = bench_result.value();
                }
            }

            // Perform analysis
            auto analysis_result = analysis_suite_->analyze_performance_parallel(returns_result.value(), benchmark);

            if (analysis_result.is_error()) {
                res.status = 500;
                res.set_content(
                    JsonSerializer::create_error_response(analysis_result.error().code, analysis_result.error().message)
                        .dump(),
                    "application/json");
                return;
            }

            // Convert analysis report to JSON
            const auto& report = analysis_result.value();
            json response_data;
            response_data["metrics"] = {
                {     "total_return",      report.total_return},
                {    "annual_return",     report.annual_return},
                {"annual_volatility", report.annual_volatility},
                {     "sharpe_ratio",      report.sharpe_ratio},
                {    "sortino_ratio",     report.sortino_ratio},
                {     "max_drawdown",      report.max_drawdown},
                {     "calmar_ratio",      report.calmar_ratio},
                {           "var_95",            report.var_95},
                {          "cvar_95",           report.cvar_95},
                {         "skewness",          report.skewness},
                {         "kurtosis",          report.kurtosis}
            };

            if (report.alpha.has_value()) {
                response_data["benchmark_metrics"] = {
                    {            "alpha",                 report.alpha.value()},
                    {             "beta",                  report.beta.value()},
                    {"information_ratio", report.information_ratio.value_or(0)},
                    {   "tracking_error",    report.tracking_error.value_or(0)}
                };
            }

            response_data["risk_analysis"] = {
                {"passed_risk_checks", report.passed_risk_checks},
                {          "warnings",           report.warnings},
                {   "recommendations",    report.recommendations}
            };

            response_data["computation_time_ms"] = report.computation_time.count();

            json response = JsonSerializer::create_api_response(true, response_data);
            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(JsonSerializer::create_error_response(ErrorCode::ServerError,
                                                                  std::string("Analysis failed: ") + e.what())
                                .dump(),
                            "application/json");
        }
    }

    /**
     * @brief Handle portfolio analysis request
     */
    void handle_portfolio_analysis(const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);

            // Parse portfolio holdings
            if (!request_data.contains("holdings") || !request_data["holdings"].is_array()) {
                res.status = 400;
                res.set_content(
                    JsonSerializer::create_error_response(ErrorCode::InvalidInput, "Missing or invalid holdings data")
                        .dump(),
                    "application/json");
                return;
            }

            positions::PortfolioHoldings holdings(DateTime::now(), request_data.value("cash", 0.0));

            for (const auto& position_data : request_data["holdings"]) {
                std::string symbol = position_data["symbol"];
                double shares      = position_data["shares"];
                double price       = position_data["price"];
                double last_price  = position_data.value("last_price", price);

                auto result = holdings.update_holding(symbol, shares, price, last_price);
                if (result.is_error()) {
                    res.status = 400;
                    res.set_content(
                        JsonSerializer::create_error_response(result.error().code, result.error().message).dump(),
                        "application/json");
                    return;
                }
            }

            // Calculate portfolio metrics
            json response_data;
            response_data["total_value"] = holdings.total_value();
            response_data["cash"]        = holdings.cash_balance();

            // Holdings details
            json holdings_array = json::array();
            for (const auto& [symbol, holding] : holdings.holdings()) {
                holdings_array.push_back(JsonSerializer::serialize_holding(holding));
            }
            response_data["holdings"] = holdings_array;

            // Portfolio metrics
            auto portfolio_metrics   = holdings.calculate_metrics();
            response_data["metrics"] = {
                {     "gross_exposure",      portfolio_metrics.gross_exposure},
                {       "net_exposure",        portfolio_metrics.net_exposure},
                {      "long_exposure",       portfolio_metrics.long_exposure},
                {     "short_exposure",      portfolio_metrics.short_exposure},
                {        "cash_weight",         portfolio_metrics.cash_weight},
                {      "num_positions",       portfolio_metrics.num_positions},
                { "num_long_positions",  portfolio_metrics.num_long_positions},
                {"num_short_positions", portfolio_metrics.num_short_positions}
            };

            json response = JsonSerializer::create_api_response(true, response_data);
            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(JsonSerializer::create_error_response(ErrorCode::ServerError,
                                                                  std::string("Portfolio analysis failed: ") + e.what())
                                .dump(),
                            "application/json");
        }
    }

    /**
     * @brief Handle transaction analysis request
     */
    void handle_transaction_analysis(const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);

            // Parse transactions
            if (!request_data.contains("transactions") || !request_data["transactions"].is_array()) {
                res.status = 400;
                res.set_content(JsonSerializer::create_error_response(ErrorCode::InvalidInput,
                                                                      "Missing or invalid transactions data")
                                    .dump(),
                                "application/json");
                return;
            }

            std::vector<transactions::TransactionRecord> txn_records;
            for (const auto& txn_data : request_data["transactions"]) {
                auto txn_result = JsonSerializer::parse_transaction_record(txn_data);
                if (txn_result.is_error()) {
                    res.status = 400;
                    res.set_content(
                        JsonSerializer::create_error_response(txn_result.error().code, txn_result.error().message)
                            .dump(),
                        "application/json");
                    return;
                }
                txn_records.push_back(txn_result.value());
            }

            // Create transaction series and analyze
            transactions::TransactionSeries txn_series(std::move(txn_records));
            auto stats_result = txn_series.calculate_statistics();

            if (stats_result.is_error()) {
                res.status = 500;
                res.set_content(
                    JsonSerializer::create_error_response(stats_result.error().code, stats_result.error().message)
                        .dump(),
                    "application/json");
                return;
            }

            const auto& stats  = stats_result.value();
            json response_data = {
                {      "total_transactions",       stats.total_transactions},
                {    "total_notional_value",     stats.total_notional_value},
                {"average_transaction_size", stats.average_transaction_size},
                {          "unique_symbols",           stats.unique_symbols},
                {            "trading_days",             stats.trading_days},
                {       "total_commissions", txn_series.total_commissions()},
                {          "total_slippage",    txn_series.total_slippage()}
            };

            json response = JsonSerializer::create_api_response(true, response_data);
            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(JsonSerializer::create_error_response(
                                ErrorCode::ServerError, std::string("Transaction analysis failed: ") + e.what())
                                .dump(),
                            "application/json");
        }
    }

    /**
     * @brief Handle Sharpe ratio calculation
     */
    void handle_sharpe_calculation(const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);

            auto returns_result = JsonSerializer::parse_time_series<double>(request_data["returns"]);
            if (returns_result.is_error()) {
                res.status = 400;
                res.set_content(
                    JsonSerializer::create_error_response(returns_result.error().code, returns_result.error().message)
                        .dump(),
                    "application/json");
                return;
            }

            double risk_free_rate = request_data.value("risk_free_rate", 0.02);

            auto sharpe_result = analytics::calculate_sharpe_ratio(returns_result.value(), risk_free_rate);

            if (sharpe_result.is_error()) {
                res.status = 500;
                res.set_content(
                    JsonSerializer::create_error_response(sharpe_result.error().code, sharpe_result.error().message)
                        .dump(),
                    "application/json");
                return;
            }

            json response_data = {
                {"sharpe_ratio", sharpe_result.value()}
            };
            json response = JsonSerializer::create_api_response(true, response_data);
            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(JsonSerializer::create_error_response(ErrorCode::ServerError,
                                                                  std::string("Sharpe calculation failed: ") + e.what())
                                .dump(),
                            "application/json");
        }
    }

    /**
     * @brief Handle drawdown calculation
     */
    void handle_drawdown_calculation(const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);

            auto prices_result = JsonSerializer::parse_time_series<double>(request_data["prices"]);
            if (prices_result.is_error()) {
                res.status = 400;
                res.set_content(
                    JsonSerializer::create_error_response(prices_result.error().code, prices_result.error().message)
                        .dump(),
                    "application/json");
                return;
            }

            auto drawdown_result = analytics::calculate_max_drawdown(prices_result.value());

            if (drawdown_result.is_error()) {
                res.status = 500;
                res.set_content(
                    JsonSerializer::create_error_response(drawdown_result.error().code, drawdown_result.error().message)
                        .dump(),
                    "application/json");
                return;
            }

            json response_data = {
                {"max_drawdown", drawdown_result.value()}
            };
            json response = JsonSerializer::create_api_response(true, response_data);
            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(JsonSerializer::create_error_response(
                                ErrorCode::ServerError, std::string("Drawdown calculation failed: ") + e.what())
                                .dump(),
                            "application/json");
        }
    }

    /**
     * @brief Handle volatility calculation
     */
    void handle_volatility_calculation(const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);

            auto returns_result = JsonSerializer::parse_time_series<double>(request_data["returns"]);
            if (returns_result.is_error()) {
                res.status = 400;
                res.set_content(
                    JsonSerializer::create_error_response(returns_result.error().code, returns_result.error().message)
                        .dump(),
                    "application/json");
                return;
            }

            int periods_per_year = request_data.value("periods_per_year", 252);

            auto vol_result = analytics::calculate_annual_volatility(returns_result.value(), periods_per_year);

            if (vol_result.is_error()) {
                res.status = 500;
                res.set_content(
                    JsonSerializer::create_error_response(vol_result.error().code, vol_result.error().message).dump(),
                    "application/json");
                return;
            }

            json response_data = {
                {"annual_volatility", vol_result.value()}
            };
            json response = JsonSerializer::create_api_response(true, response_data);
            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(JsonSerializer::create_error_response(
                                ErrorCode::ServerError, std::string("Volatility calculation failed: ") + e.what())
                                .dump(),
                            "application/json");
        }
    }

    /**
     * @brief Handle batch analysis request
     */
    void handle_batch_analysis(const httplib::Request& req, httplib::Response& res) {
        try {
            json request_data = json::parse(req.body);

            if (!request_data.contains("portfolios") || !request_data["portfolios"].is_array()) {
                res.status = 400;
                res.set_content(
                    JsonSerializer::create_error_response(ErrorCode::InvalidInput, "Missing or invalid portfolios data")
                        .dump(),
                    "application/json");
                return;
            }

            json results_array = json::array();

            for (const auto& portfolio_data : request_data["portfolios"]) {
                std::string portfolio_id = portfolio_data.value("id", "unknown");

                auto returns_result = JsonSerializer::parse_time_series<double>(portfolio_data["returns"]);
                if (returns_result.is_error()) {
                    results_array.push_back({
                        {     "id",                   portfolio_id},
                        {"success",                          false},
                        {  "error", returns_result.error().message}
                    });
                    continue;
                }

                auto analysis_result = analysis_suite_->analyze_performance_parallel(returns_result.value());

                if (analysis_result.is_error()) {
                    results_array.push_back({
                        {     "id",                    portfolio_id},
                        {"success",                           false},
                        {  "error", analysis_result.error().message}
                    });
                    continue;
                }

                const auto& report = analysis_result.value();
                results_array.push_back({
                    {     "id",portfolio_id                               },
                    {"success",                         true},
                    {"metrics",
                     {{"sharpe_ratio", report.sharpe_ratio},
                     {"annual_return", report.annual_return},
                     {"annual_volatility", report.annual_volatility},
                     {"max_drawdown", report.max_drawdown}} }
                });
            }

            json response_data = {
                {"results", results_array}
            };
            json response = JsonSerializer::create_api_response(true, response_data);
            res.set_content(response.dump(), "application/json");

        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(JsonSerializer::create_error_response(ErrorCode::ServerError,
                                                                  std::string("Batch analysis failed: ") + e.what())
                                .dump(),
                            "application/json");
        }
    }

    /**
     * @brief Start the server
     */
    void start() {
        std::cout << "Starting REST API server on " << config_.host << ":" << config_.port << std::endl;
        std::cout << "API base path: " << config_.base_path << std::endl;
        server_.listen(config_.host.c_str(), config_.port);
    }

    /**
     * @brief Stop the server
     */
    void stop() { server_.stop(); }

    /**
     * @brief Check if server is running
     */
    bool is_running() const { return server_.is_running(); }
};

}  // namespace pyfolio::web