#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/chrono.h>
#include <pybind11/eigen.h>
#include <pybind11/functional.h>

// Core includes
#include "pyfolio/core/types.h"
#include "pyfolio/core/datetime.h"
#include "pyfolio/core/time_series.h"
#include "pyfolio/core/error_handling.h"

// Analytics includes
#include "pyfolio/analytics/performance_metrics.h"
#include "pyfolio/analytics/statistics.h"
#include "pyfolio/analytics/risk_analysis.h"
#include "pyfolio/analytics/rolling_metrics.h"

// Backtesting includes
#include "pyfolio/backtesting/advanced_backtester.h"
#include "pyfolio/backtesting/strategies.h"

// Visualization includes
#include "pyfolio/visualization/plotly_enhanced.h"

// ML includes
#include "pyfolio/analytics/ml_regime_detection.h"

// Real-time includes
#include "pyfolio/streaming/real_time_analyzer.h"

namespace py = pybind11;
using namespace pyfolio;

/**
 * @brief Convert pyfolio Result<T> to Python exception or value
 */
template<typename T>
T result_to_python(const Result<T>& result) {
    if (result.is_error()) {
        throw std::runtime_error(result.error().message);
    }
    return result.value();
}

/**
 * @brief Convert numpy array to TimeSeries
 */
template<typename T>
TimeSeries<T> numpy_to_timeseries(py::array_t<double> timestamps, py::array_t<T> values, const std::string& name = "") {
    auto ts_buf = timestamps.request();
    auto val_buf = values.request();
    
    if (ts_buf.size != val_buf.size) {
        throw std::runtime_error("Timestamps and values must have the same length");
    }
    
    std::vector<DateTime> dt_vec;
    std::vector<T> val_vec;
    
    double* ts_ptr = static_cast<double*>(ts_buf.ptr);
    T* val_ptr = static_cast<T*>(val_buf.ptr);
    
    for (py::ssize_t i = 0; i < ts_buf.size; ++i) {
        // Assume timestamps are Unix timestamps
        dt_vec.push_back(DateTime::from_timestamp(static_cast<time_t>(ts_ptr[i])));
        val_vec.push_back(val_ptr[i]);
    }
    
    auto result = TimeSeries<T>::create(dt_vec, val_vec, name);
    return result_to_python(result);
}

/**
 * @brief Convert TimeSeries to numpy arrays
 */
template<typename T>
std::pair<py::array_t<double>, py::array_t<T>> timeseries_to_numpy(const TimeSeries<T>& ts) {
    const auto& timestamps = ts.timestamps();
    const auto& values = ts.values();
    
    auto ts_array = py::array_t<double>(timestamps.size());
    auto val_array = py::array_t<T>(values.size());
    
    double* ts_ptr = static_cast<double*>(ts_array.mutable_unchecked<1>().mutable_data(0));
    T* val_ptr = static_cast<T*>(val_array.mutable_unchecked<1>().mutable_data(0));
    
    for (size_t i = 0; i < timestamps.size(); ++i) {
        ts_ptr[i] = static_cast<double>(timestamps[i].timestamp());
        val_ptr[i] = values[i];
    }
    
    return {ts_array, val_array};
}

PYBIND11_MODULE(pyfolio_cpp, m) {
    m.doc() = "PyFolio C++ - High-performance portfolio analytics library";
    
    // ===== Core Types =====
    py::enum_<ErrorCode>(m, "ErrorCode")
        .value("Success", ErrorCode::Success)
        .value("InvalidInput", ErrorCode::InvalidInput)
        .value("InsufficientData", ErrorCode::InsufficientData)
        .value("ComputationError", ErrorCode::ComputationError)
        .value("NetworkError", ErrorCode::NetworkError)
        .value("FileError", ErrorCode::FileError)
        .export_values();
    
    py::enum_<TransactionSide>(m, "TransactionSide")
        .value("Buy", TransactionSide::Buy)
        .value("Sell", TransactionSide::Sell)
        .export_values();
    
    py::class_<DateTime>(m, "DateTime")
        .def(py::init<int, int, int>(), "year"_a, "month"_a, "day"_a)
        .def(py::init<int, int, int, int, int, int>(), 
             "year"_a, "month"_a, "day"_a, "hour"_a, "minute"_a, "second"_a)
        .def_static("now", &DateTime::now)
        .def_static("from_timestamp", &DateTime::from_timestamp)
        .def("timestamp", &DateTime::timestamp)
        .def("to_string", &DateTime::to_string)
        .def("add_days", &DateTime::add_days)
        .def("add_months", &DateTime::add_months)
        .def("year", &DateTime::year)
        .def("month", &DateTime::month)
        .def("day", &DateTime::day)
        .def("__str__", &DateTime::to_string)
        .def("__repr__", [](const DateTime& dt) { return "DateTime('" + dt.to_string() + "')"; });
    
    py::class_<Position>(m, "Position")
        .def(py::init<>())
        .def_readwrite("shares", &Position::shares)
        .def_readwrite("price", &Position::price)
        .def_readwrite("timestamp", &Position::timestamp)
        .def("market_value", &Position::market_value);
    
    // ===== TimeSeries =====
    py::class_<TimeSeries<double>>(m, "TimeSeries")
        .def(py::init<>())
        .def(py::init<const std::string&>())
        .def_static("create", [](py::array_t<double> timestamps, py::array_t<double> values, const std::string& name) {
            return numpy_to_timeseries<double>(timestamps, values, name);
        }, "timestamps"_a, "values"_a, "name"_a = "")
        .def("size", &TimeSeries<double>::size)
        .def("empty", &TimeSeries<double>::empty)
        .def("name", &TimeSeries<double>::name)
        .def("set_name", &TimeSeries<double>::set_name)
        .def("to_numpy", [](const TimeSeries<double>& ts) {
            return timeseries_to_numpy(ts);
        })
        .def("__len__", &TimeSeries<double>::size)
        .def("__getitem__", [](const TimeSeries<double>& ts, size_t idx) {
            if (idx >= ts.size()) throw py::index_error();
            return std::make_pair(ts.timestamps()[idx], ts.values()[idx]);
        });
    
    py::class_<TimeSeries<Price>>(m, "PriceTimeSeries")
        .def(py::init<>())
        .def_static("create", [](py::array_t<double> timestamps, py::array_t<double> values, const std::string& name) {
            return numpy_to_timeseries<Price>(timestamps, values, name);
        }, "timestamps"_a, "values"_a, "name"_a = "")
        .def("size", &TimeSeries<Price>::size)
        .def("to_numpy", [](const TimeSeries<Price>& ts) {
            return timeseries_to_numpy(ts);
        });
    
    // ===== Performance Metrics =====
    py::class_<analytics::PerformanceMetrics>(m, "PerformanceMetrics")
        .def(py::init<>())
        .def_readwrite("total_return", &analytics::PerformanceMetrics::total_return)
        .def_readwrite("annual_return", &analytics::PerformanceMetrics::annual_return)
        .def_readwrite("annual_volatility", &analytics::PerformanceMetrics::annual_volatility)
        .def_readwrite("sharpe_ratio", &analytics::PerformanceMetrics::sharpe_ratio)
        .def_readwrite("sortino_ratio", &analytics::PerformanceMetrics::sortino_ratio)
        .def_readwrite("calmar_ratio", &analytics::PerformanceMetrics::calmar_ratio)
        .def_readwrite("max_drawdown", &analytics::PerformanceMetrics::max_drawdown)
        .def_readwrite("win_rate", &analytics::PerformanceMetrics::win_rate)
        .def_readwrite("profit_factor", &analytics::PerformanceMetrics::profit_factor);
    
    // ===== Analytics Functions =====
    py::module_ analytics = m.def_submodule("analytics", "Portfolio analytics functions");
    
    analytics.def("calculate_performance_metrics", [](const TimeSeries<double>& returns) {
        auto result = analytics::calculate_performance_metrics(returns);
        return result_to_python(result);
    }, "Calculate comprehensive performance metrics from returns time series");
    
    analytics.def("calculate_sharpe_ratio", [](const TimeSeries<double>& returns, double risk_free_rate) {
        auto result = analytics::calculate_sharpe_ratio(returns, risk_free_rate);
        return result_to_python(result);
    }, "Calculate Sharpe ratio", "returns"_a, "risk_free_rate"_a = 0.0);
    
    analytics.def("calculate_max_drawdown", [](const TimeSeries<double>& returns) {
        auto result = analytics::calculate_max_drawdown(returns);
        return result_to_python(result);
    }, "Calculate maximum drawdown");
    
    analytics.def("calculate_var", [](const TimeSeries<double>& returns, double confidence_level) {
        auto result = analytics::calculate_var(returns, confidence_level);
        return result_to_python(result);
    }, "Calculate Value at Risk", "returns"_a, "confidence_level"_a = 0.05);
    
    analytics.def("calculate_cvar", [](const TimeSeries<double>& returns, double confidence_level) {
        auto result = analytics::calculate_cvar(returns, confidence_level);
        return result_to_python(result);
    }, "Calculate Conditional Value at Risk", "returns"_a, "confidence_level"_a = 0.05);
    
    // ===== Rolling Metrics =====
    analytics.def("rolling_sharpe", [](const TimeSeries<double>& returns, size_t window, double risk_free_rate) {
        auto result = analytics::rolling_sharpe_ratio(returns, window, risk_free_rate);
        return result_to_python(result);
    }, "Calculate rolling Sharpe ratio", "returns"_a, "window"_a, "risk_free_rate"_a = 0.0);
    
    analytics.def("rolling_volatility", [](const TimeSeries<double>& returns, size_t window) {
        auto result = analytics::rolling_volatility(returns, window);
        return result_to_python(result);
    }, "Calculate rolling volatility");
    
    analytics.def("rolling_beta", [](const TimeSeries<double>& returns, const TimeSeries<double>& benchmark, size_t window) {
        auto result = analytics::rolling_beta(returns, benchmark, window);
        return result_to_python(result);
    }, "Calculate rolling beta");
    
    // ===== Backtesting =====
    py::module_ backtesting = m.def_submodule("backtesting", "Backtesting framework");
    
    py::enum_<backtesting::CommissionType>(backtesting, "CommissionType")
        .value("Fixed", backtesting::CommissionType::Fixed)
        .value("Percentage", backtesting::CommissionType::Percentage)
        .value("PerShare", backtesting::CommissionType::PerShare)
        .value("Tiered", backtesting::CommissionType::Tiered)
        .export_values();
    
    py::enum_<backtesting::MarketImpactModel>(backtesting, "MarketImpactModel")
        .value("None", backtesting::MarketImpactModel::None)
        .value("Linear", backtesting::MarketImpactModel::Linear)
        .value("SquareRoot", backtesting::MarketImpactModel::SquareRoot)
        .value("Almgren", backtesting::MarketImpactModel::Almgren)
        .value("Custom", backtesting::MarketImpactModel::Custom)
        .export_values();
    
    py::class_<backtesting::CommissionStructure>(backtesting, "CommissionStructure")
        .def(py::init<>())
        .def_readwrite("type", &backtesting::CommissionStructure::type)
        .def_readwrite("rate", &backtesting::CommissionStructure::rate)
        .def_readwrite("minimum", &backtesting::CommissionStructure::minimum)
        .def_readwrite("maximum", &backtesting::CommissionStructure::maximum)
        .def("calculate_commission", &backtesting::CommissionStructure::calculate_commission);
    
    py::class_<backtesting::MarketImpactConfig>(backtesting, "MarketImpactConfig")
        .def(py::init<>())
        .def_readwrite("model", &backtesting::MarketImpactConfig::model)
        .def_readwrite("impact_coefficient", &backtesting::MarketImpactConfig::impact_coefficient)
        .def_readwrite("permanent_impact_ratio", &backtesting::MarketImpactConfig::permanent_impact_ratio)
        .def_readwrite("participation_rate", &backtesting::MarketImpactConfig::participation_rate)
        .def("calculate_impact", &backtesting::MarketImpactConfig::calculate_impact);
    
    py::class_<backtesting::SlippageConfig>(backtesting, "SlippageConfig")
        .def(py::init<>())
        .def_readwrite("bid_ask_spread", &backtesting::SlippageConfig::bid_ask_spread)
        .def_readwrite("volatility_multiplier", &backtesting::SlippageConfig::volatility_multiplier)
        .def_readwrite("enable_random_slippage", &backtesting::SlippageConfig::enable_random_slippage);
    
    py::class_<backtesting::BacktestConfig>(backtesting, "BacktestConfig")
        .def(py::init<>())
        .def_readwrite("start_date", &backtesting::BacktestConfig::start_date)
        .def_readwrite("end_date", &backtesting::BacktestConfig::end_date)
        .def_readwrite("initial_capital", &backtesting::BacktestConfig::initial_capital)
        .def_readwrite("commission", &backtesting::BacktestConfig::commission)
        .def_readwrite("market_impact", &backtesting::BacktestConfig::market_impact)
        .def_readwrite("slippage", &backtesting::BacktestConfig::slippage)
        .def_readwrite("enable_partial_fills", &backtesting::BacktestConfig::enable_partial_fills)
        .def_readwrite("cash_buffer", &backtesting::BacktestConfig::cash_buffer)
        .def_readwrite("max_position_size", &backtesting::BacktestConfig::max_position_size);
    
    py::class_<backtesting::ExecutedTrade>(backtesting, "ExecutedTrade")
        .def(py::init<>())
        .def_readwrite("timestamp", &backtesting::ExecutedTrade::timestamp)
        .def_readwrite("symbol", &backtesting::ExecutedTrade::symbol)
        .def_readwrite("quantity", &backtesting::ExecutedTrade::quantity)
        .def_readwrite("execution_price", &backtesting::ExecutedTrade::execution_price)
        .def_readwrite("market_price", &backtesting::ExecutedTrade::market_price)
        .def_readwrite("commission", &backtesting::ExecutedTrade::commission)
        .def_readwrite("market_impact", &backtesting::ExecutedTrade::market_impact)
        .def_readwrite("slippage", &backtesting::ExecutedTrade::slippage)
        .def_readwrite("total_cost", &backtesting::ExecutedTrade::total_cost)
        .def("implementation_shortfall", &backtesting::ExecutedTrade::implementation_shortfall);
    
    py::class_<backtesting::BacktestResults>(backtesting, "BacktestResults")
        .def(py::init<>())
        .def_readwrite("start_date", &backtesting::BacktestResults::start_date)
        .def_readwrite("end_date", &backtesting::BacktestResults::end_date)
        .def_readwrite("initial_capital", &backtesting::BacktestResults::initial_capital)
        .def_readwrite("final_value", &backtesting::BacktestResults::final_value)
        .def_readwrite("performance", &backtesting::BacktestResults::performance)
        .def_readwrite("total_commission", &backtesting::BacktestResults::total_commission)
        .def_readwrite("total_market_impact", &backtesting::BacktestResults::total_market_impact)
        .def_readwrite("total_slippage", &backtesting::BacktestResults::total_slippage)
        .def_readwrite("total_trades", &backtesting::BacktestResults::total_trades)
        .def_readwrite("max_drawdown", &backtesting::BacktestResults::max_drawdown)
        .def_readwrite("sharpe_ratio", &backtesting::BacktestResults::sharpe_ratio)
        .def_readwrite("trade_history", &backtesting::BacktestResults::trade_history)
        .def("generate_report", &backtesting::BacktestResults::generate_report);
    
    py::class_<backtesting::AdvancedBacktester>(backtesting, "AdvancedBacktester")
        .def(py::init<const backtesting::BacktestConfig&>())
        .def("load_price_data", [](backtesting::AdvancedBacktester& self, const std::string& symbol, const TimeSeries<Price>& prices) {
            auto result = self.load_price_data(symbol, prices);
            if (result.is_error()) {
                throw std::runtime_error(result.error().message);
            }
        })
        .def("run_backtest", [](backtesting::AdvancedBacktester& self) {
            auto result = self.run_backtest();
            return result_to_python(result);
        })
        .def("get_trade_history", &backtesting::AdvancedBacktester::get_trade_history);
    
    // ===== Strategies =====
    py::class_<backtesting::strategies::BuyAndHoldStrategy>(backtesting, "BuyAndHoldStrategy")
        .def(py::init<const std::vector<std::string>&>())
        .def("get_name", &backtesting::strategies::BuyAndHoldStrategy::get_name);
    
    py::class_<backtesting::strategies::EqualWeightStrategy>(backtesting, "EqualWeightStrategy")
        .def(py::init<const std::vector<std::string>&, size_t>(), "symbols"_a, "rebalance_frequency"_a = 21)
        .def("get_name", &backtesting::strategies::EqualWeightStrategy::get_name);
    
    py::class_<backtesting::strategies::MomentumStrategy>(backtesting, "MomentumStrategy")
        .def(py::init<const std::vector<std::string>&, size_t, size_t>(), 
             "symbols"_a, "lookback_period"_a = 20, "top_n"_a = 1)
        .def("get_name", &backtesting::strategies::MomentumStrategy::get_name);
    
    // ===== Machine Learning =====
    py::module_ ml = m.def_submodule("ml", "Machine learning regime detection");
    
    py::enum_<analytics::RegimeDetectionMethod>(ml, "RegimeDetectionMethod")
        .value("MarkovSwitching", analytics::RegimeDetectionMethod::MarkovSwitching)
        .value("HMM", analytics::RegimeDetectionMethod::HMM)
        .value("NeuralNetwork", analytics::RegimeDetectionMethod::NeuralNetwork)
        .value("RandomForest", analytics::RegimeDetectionMethod::RandomForest)
        .value("SVM", analytics::RegimeDetectionMethod::SVM)
        .value("Ensemble", analytics::RegimeDetectionMethod::Ensemble)
        .export_values();
    
    py::class_<analytics::MLRegimeDetector>(ml, "MLRegimeDetector")
        .def(py::init<analytics::RegimeDetectionMethod>())
        .def("detect_regimes", [](analytics::MLRegimeDetector& self, const TimeSeries<double>& returns) {
            auto result = self.detect_regimes(returns);
            return result_to_python(result);
        })
        .def("get_regime_probabilities", [](analytics::MLRegimeDetector& self, const TimeSeries<double>& returns) {
            auto result = self.get_regime_probabilities(returns);
            return result_to_python(result);
        });
    
    // ===== Visualization =====
    py::module_ viz = m.def_submodule("visualization", "Enhanced visualization capabilities");
    
    py::class_<visualization::PlotlyEnhanced>(viz, "PlotlyEnhanced")
        .def_static("create_performance_dashboard", [](const backtesting::BacktestResults& results, const std::string& output_path) {
            auto result = visualization::PlotlyEnhanced::create_performance_dashboard(results, output_path);
            if (result.is_error()) {
                throw std::runtime_error(result.error().message);
            }
        })
        .def_static("create_returns_analysis", [](const TimeSeries<double>& returns, const std::string& output_path) {
            auto result = visualization::PlotlyEnhanced::create_returns_analysis(returns, output_path);
            if (result.is_error()) {
                throw std::runtime_error(result.error().message);
            }
        })
        .def_static("create_risk_dashboard", [](const TimeSeries<double>& returns, const std::string& output_path) {
            auto result = visualization::PlotlyEnhanced::create_risk_dashboard(returns, output_path);
            if (result.is_error()) {
                throw std::runtime_error(result.error().message);
            }
        });
    
    // ===== Real-time Analysis =====
    py::module_ streaming = m.def_submodule("streaming", "Real-time streaming analysis");
    
    py::class_<streaming::RealTimeAnalyzer>(streaming, "RealTimeAnalyzer")
        .def(py::init<const streaming::StreamingConfig&>())
        .def("add_data_point", [](streaming::RealTimeAnalyzer& self, const std::string& symbol, const DateTime& timestamp, double value) {
            auto result = self.add_data_point(symbol, timestamp, value);
            if (result.is_error()) {
                throw std::runtime_error(result.error().message);
            }
        })
        .def("get_current_metrics", [](streaming::RealTimeAnalyzer& self, const std::string& symbol) {
            auto result = self.get_current_metrics(symbol);
            return result_to_python(result);
        })
        .def("start_processing", &streaming::RealTimeAnalyzer::start_processing)
        .def("stop_processing", &streaming::RealTimeAnalyzer::stop_processing);
    
    // ===== Utility Functions =====
    m.def("version", []() {
        return "1.0.0";
    }, "Get library version");
    
    m.def("create_sample_data", [](const DateTime& start, const DateTime& end, double initial_value, double volatility) {
        std::vector<DateTime> dates;
        std::vector<double> values;
        
        auto current = start;
        double current_value = initial_value;
        std::mt19937 rng(42);
        std::normal_distribution<double> dist(0.0, volatility);
        
        while (current <= end) {
            dates.push_back(current);
            values.push_back(current_value);
            
            current_value *= (1.0 + dist(rng));
            current = current.add_days(1);
        }
        
        auto result = TimeSeries<double>::create(dates, values, "sample_data");
        return result_to_python(result);
    }, "Create sample financial data for testing", 
       "start"_a, "end"_a, "initial_value"_a = 100.0, "volatility"_a = 0.02);
}