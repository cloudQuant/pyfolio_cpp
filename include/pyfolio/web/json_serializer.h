#pragma once

/**
 * @file json_serializer.h
 * @brief JSON serialization for pyfolio data types
 * @version 1.0.0
 * @date 2024
 *
 * This file provides JSON serialization capabilities for pyfolio data types
 * to support REST API and web visualization.
 */

#include "../analytics/performance_metrics.h"
#include "../core/error_handling.h"
#include "../core/time_series.h"
#include "../core/types.h"
#include "../positions/positions.h"
#include "../transactions/transaction.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace pyfolio::web {

using json = nlohmann::json;

/**
 * @brief JSON serialization helpers for pyfolio types
 */
class JsonSerializer {
  public:
    /**
     * @brief Serialize DateTime to ISO 8601 string
     */
    static json serialize_datetime(const DateTime& dt) { return dt.to_string("%Y-%m-%dT%H:%M:%SZ"); }

    /**
     * @brief Serialize TimeSeries to JSON
     */
    template <typename T>
    static json serialize_time_series(const TimeSeries<T>& series) {
        json result;
        result["name"] = series.name();
        result["size"] = series.size();

        json data = json::array();
        for (size_t i = 0; i < series.size(); ++i) {
            json point;
            point["timestamp"] = serialize_datetime(series.timestamps()[i]);
            point["value"]     = series.values()[i];
            data.push_back(point);
        }
        result["data"] = data;

        return result;
    }

    /**
     * @brief Serialize PerformanceMetrics to JSON
     */
    static json serialize_performance_metrics(const analytics::PerformanceMetrics& metrics) {
        json result;

        result["total_return"]       = metrics.total_return;
        result["annual_return"]      = metrics.annual_return;
        result["annual_volatility"]  = metrics.annual_volatility;
        result["sharpe_ratio"]       = metrics.sharpe_ratio;
        result["sortino_ratio"]      = metrics.sortino_ratio;
        result["calmar_ratio"]       = metrics.calmar_ratio;
        result["max_drawdown"]       = metrics.max_drawdown;
        result["var_95"]             = metrics.var_95;
        result["var_99"]             = metrics.var_99;
        result["information_ratio"]  = metrics.information_ratio;
        result["tail_ratio"]         = metrics.tail_ratio;
        result["common_sense_ratio"] = metrics.common_sense_ratio;
        result["skewness"]           = metrics.skewness;
        result["kurtosis"]           = metrics.kurtosis;
        result["alpha"]              = metrics.alpha;
        result["beta"]               = metrics.beta;
        result["omega_ratio"]        = metrics.omega_ratio;
        result["stability"]          = metrics.stability;
        result["downside_deviation"] = metrics.downside_deviation;
        result["tracking_error"]     = metrics.tracking_error;

        return result;
    }

    /**
     * @brief Serialize Holding to JSON
     */
    static json serialize_holding(const positions::Holding& holding) {
        json result;

        result["symbol"]         = holding.symbol;
        result["shares"]         = holding.shares;
        result["average_cost"]   = holding.average_cost;
        result["current_price"]  = holding.current_price;
        result["market_value"]   = holding.market_value;
        result["cost_basis"]     = holding.cost_basis;
        result["unrealized_pnl"] = holding.unrealized_pnl;
        result["weight"]         = holding.weight;
        result["return_pct"]     = holding.return_pct();

        return result;
    }

    /**
     * @brief Serialize PortfolioHoldings to JSON
     */
    static json serialize_portfolio_holdings(const positions::PortfolioHoldings& holdings) {
        json result;

        result["timestamp"]   = serialize_datetime(holdings.timestamp());
        result["cash"]        = holdings.cash_balance();
        result["total_value"] = holdings.total_value();

        json holdings_json = json::array();
        for (const auto& [symbol, holding] : holdings.holdings()) {
            json holding_json = serialize_holding(holding);
            holdings_json.push_back(holding_json);
        }
        result["holdings"] = holdings_json;

        return result;
    }

    /**
     * @brief Serialize TransactionRecord to JSON
     */
    static json serialize_transaction_record(const transactions::TransactionRecord& txn) {
        json result;

        result["symbol"]        = txn.symbol();
        result["timestamp"]     = serialize_datetime(txn.timestamp());
        result["shares"]        = txn.shares();
        result["price"]         = txn.price();
        result["type"]          = txn.is_buy() ? "buy" : "sell";
        result["value"]         = txn.value();
        result["commission"]    = txn.commission();
        result["slippage"]      = txn.slippage();
        result["exchange"]      = txn.exchange();
        result["order_id"]      = txn.order_id();
        result["net_cash_flow"] = txn.net_cash_flow();

        return result;
    }

    /**
     * @brief Serialize Result<T> to JSON with error handling
     */
    template <typename T>
    static json serialize_result(const Result<T>& result, std::function<json(const T&)> value_serializer) {
        json response;

        if (result.is_ok()) {
            response["success"] = true;
            response["data"]    = value_serializer(result.value());
        } else {
            response["success"] = false;
            response["error"]   = {
                {   "code", static_cast<int>(result.error().code)},
                {"message",                result.error().message}
            };
        }

        return response;
    }

    /**
     * @brief Create a standard API response
     */
    static json create_api_response(bool success, const json& data = json{}, const std::string& message = "") {
        json response;
        response["success"]   = success;
        response["timestamp"] = DateTime::now().to_string("%Y-%m-%dT%H:%M:%SZ");

        if (!data.empty()) {
            response["data"] = data;
        }

        if (!message.empty()) {
            response["message"] = message;
        }

        return response;
    }

    /**
     * @brief Create error response
     */
    static json create_error_response(ErrorCode code, const std::string& message) {
        return create_api_response(false, json{}, message);
    }

    /**
     * @brief Parse TimeSeries from JSON
     */
    template <typename T>
    static Result<TimeSeries<T>> parse_time_series(const json& data) {
        try {
            if (!data.contains("data") || !data["data"].is_array()) {
                return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput,
                                                    "Invalid JSON: missing or invalid 'data' field");
            }

            std::vector<DateTime> timestamps;
            std::vector<T> values;

            for (const auto& point : data["data"]) {
                if (!point.contains("timestamp") || !point.contains("value")) {
                    return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput,
                                                        "Invalid data point: missing timestamp or value");
                }

                auto dt_result = DateTime::parse(point["timestamp"].get<std::string>());
                if (dt_result.is_error()) {
                    return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput, "Invalid timestamp format");
                }

                timestamps.push_back(dt_result.value());
                values.push_back(point["value"].get<T>());
            }

            std::string name = data.value("name", "series");
            return TimeSeries<T>::create(timestamps, values, name);

        } catch (const std::exception& e) {
            return Result<TimeSeries<T>>::error(ErrorCode::InvalidInput,
                                                std::string("JSON parsing error: ") + e.what());
        }
    }

    /**
     * @brief Parse TransactionRecord from JSON
     */
    static Result<transactions::TransactionRecord> parse_transaction_record(const json& data) {
        try {
            if (!data.contains("symbol") || !data.contains("timestamp") || !data.contains("shares") ||
                !data.contains("price")) {
                return Result<transactions::TransactionRecord>::error(ErrorCode::InvalidInput,
                                                                      "Missing required transaction fields");
            }

            std::string symbol = data["symbol"].get<std::string>();

            auto dt_result = DateTime::parse(data["timestamp"].get<std::string>());
            if (dt_result.is_error()) {
                return Result<transactions::TransactionRecord>::error(ErrorCode::InvalidInput,
                                                                      "Invalid timestamp format");
            }

            double shares = data["shares"].get<double>();
            double price  = data["price"].get<double>();

            // Determine transaction type
            transactions::TransactionType type =
                shares > 0 ? transactions::TransactionType::Buy : transactions::TransactionType::Sell;

            if (data.contains("type")) {
                std::string type_str = data["type"].get<std::string>();
                if (type_str == "sell") {
                    type = transactions::TransactionType::Sell;
                } else {
                    type = transactions::TransactionType::Buy;
                }
            }

            std::string currency = data.value("currency", "USD");
            double commission    = data.value("commission", 0.0);
            double slippage      = data.value("slippage", 0.0);

            // Create transaction record
            transactions::TransactionRecord txn(symbol, shares, price, dt_result.value(), type, currency, commission,
                                                slippage);

            if (data.contains("exchange")) {
                txn.set_exchange(data["exchange"].get<std::string>());
            }

            if (data.contains("order_id")) {
                txn.set_order_id(data["order_id"].get<std::string>());
            }

            return Result<transactions::TransactionRecord>::success(txn);

        } catch (const std::exception& e) {
            return Result<transactions::TransactionRecord>::error(ErrorCode::InvalidInput,
                                                                  std::string("JSON parsing error: ") + e.what());
        }
    }
};

// Note: Custom JSON serializers would go here if needed, but we'll use
// explicit serialization methods instead to avoid namespace issues

}  // namespace pyfolio::web