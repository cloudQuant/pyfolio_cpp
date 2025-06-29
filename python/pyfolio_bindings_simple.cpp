#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/chrono.h>

// Core includes
#include "pyfolio/core/types.h"
#include "pyfolio/core/datetime.h"
#include "pyfolio/core/time_series.h"
#include "pyfolio/core/error_handling.h"

// Analytics includes
#include "pyfolio/analytics/performance_metrics.h"
#include "pyfolio/analytics/statistics.h"

// Backtesting includes (simplified)
#include "pyfolio/backtesting/advanced_backtester.h"

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