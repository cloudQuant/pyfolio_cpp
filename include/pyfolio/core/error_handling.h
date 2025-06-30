#pragma once

#include <exception>
#include <functional>
#include <optional>
#include <source_location>
#include <string>
#include <variant>

namespace pyfolio {

/**
 * @brief Error categories for financial calculations
 */
enum class ErrorCode {
    Success,
    InvalidInput,
    InsufficientData,
    DivisionByZero,
    NumericOverflow,
    NumericUnderflow,
    MissingData,
    InvalidDateRange,
    InvalidSymbol,
    InvalidCurrency,
    CalculationError,
    InvalidState,
    FileNotFound,
    NotFound,
    ParseError,
    NetworkError,
    MemoryError,
    BufferOverflow,
    UnknownError
};

/**
 * @brief Detailed error information
 */
struct Error {
    ErrorCode code;
    std::string message;
    std::string context;
    std::source_location location;

    Error(ErrorCode c, const std::string& msg, const std::string& ctx = "",
          std::source_location loc = std::source_location::current())
        : code(c), message(msg), context(ctx), location(loc) {}

    // Convenience constructor for simple string errors
    explicit Error(const std::string& msg, std::source_location loc = std::source_location::current())
        : code(ErrorCode::UnknownError), message(msg), context(""), location(loc) {}

    std::string to_string() const {
        return std::string("[") + location.file_name() + ":" + std::to_string(location.line()) + "] " + message +
               (context.empty() ? "" : " (" + context + ")");
    }
};

/**
 * @brief Result<T> monad for robust error handling without exceptions
 */
template <typename T>
class Result {
  private:
    std::variant<T, Error> data_;

  public:
    // Constructors
    Result(const T& value) : data_(value) {}
    Result(T&& value) : data_(std::move(value)) {}
    Result(const Error& error) : data_(error) {}
    Result(Error&& error) : data_(std::move(error)) {}

    // Success factory
    static Result success(const T& value) { return Result{value}; }

    static Result success(T&& value) { return Result{std::move(value)}; }

    // Error factory
    static Result error(ErrorCode code, const std::string& message, const std::string& context = "",
                        std::source_location loc = std::source_location::current()) {
        return Result{
            Error{code, message, context, loc}
        };
    }

    // State queries
    bool is_ok() const noexcept { return std::holds_alternative<T>(data_); }

    bool is_error() const noexcept { return std::holds_alternative<Error>(data_); }

    // Optional-style interface for compatibility
    bool has_value() const noexcept { return is_ok(); }

    // Value access
    const T& value() const {
        if (is_error()) {
            throw std::runtime_error("Attempted to access value of error result: " +
                                     std::get<Error>(data_).to_string());
        }
        return std::get<T>(data_);
    }

    T& value() {
        if (is_error()) {
            throw std::runtime_error("Attempted to access value of error result: " +
                                     std::get<Error>(data_).to_string());
        }
        return std::get<T>(data_);
    }

    const Error& error() const {
        if (is_ok()) {
            throw std::runtime_error("Attempted to access error of successful result");
        }
        return std::get<Error>(data_);
    }

    // Safe value access
    T value_or(const T& default_value) const { return is_ok() ? std::get<T>(data_) : default_value; }

    // Monadic operations
    template <typename F>
    auto map(F&& func) const -> Result<decltype(func(std::declval<T>()))> {
        using U = decltype(func(std::declval<T>()));

        if (is_error()) {
            return Result<U>{std::get<Error>(data_)};
        }

        try {
            return Result<U>::success(func(std::get<T>(data_)));
        } catch (const std::exception& e) {
            return Result<U>::error(ErrorCode::CalculationError, e.what());
        }
    }

    template <typename F>
    auto and_then(F&& func) const -> decltype(func(std::declval<T>())) {
        using ResultType = decltype(func(std::declval<T>()));

        if (is_error()) {
            return ResultType{std::get<Error>(data_)};
        }

        try {
            return func(std::get<T>(data_));
        } catch (const std::exception& e) {
            return ResultType::error(ErrorCode::CalculationError, e.what());
        }
    }

    template <typename F>
    Result or_else(F&& func) const {
        if (is_ok()) {
            return *this;
        }

        return func(std::get<Error>(data_));
    }
};

/**
 * @brief Specialization for Result<void>
 */
template <>
class Result<void> {
  private:
    std::optional<Error> error_;

  public:
    // Constructors
    Result() : error_(std::nullopt) {}
    Result(const Error& error) : error_(error) {}
    Result(Error&& error) : error_(std::move(error)) {}

    // Success factory
    static Result success() { return Result{}; }

    // Error factory
    static Result error(ErrorCode code, const std::string& message, const std::string& context = "",
                        std::source_location loc = std::source_location::current()) {
        return Result{
            Error{code, message, context, loc}
        };
    }

    // State queries
    bool is_ok() const noexcept { return !error_.has_value(); }

    bool is_error() const noexcept { return error_.has_value(); }

    // Optional-style interface for compatibility
    bool has_value() const noexcept { return is_ok(); }

    // Error access
    const Error& error() const {
        if (is_ok()) {
            throw std::runtime_error("Attempted to access error of successful result");
        }
        return *error_;
    }

    // Monadic operations
    template <typename F>
    auto and_then(F&& func) const -> decltype(func()) {
        using ResultType = decltype(func());

        if (is_error()) {
            return ResultType{*error_};
        }

        try {
            return func();
        } catch (const std::exception& e) {
            return ResultType::error(ErrorCode::CalculationError, e.what());
        }
    }

    template <typename F>
    Result or_else(F&& func) const {
        if (is_ok()) {
            return *this;
        }

        return func(*error_);
    }
};

// Convenience macros for error handling
#define PYFOLIO_TRY(expr)                                                                                              \
    do {                                                                                                               \
        auto result = (expr);                                                                                          \
        if (result.is_error()) {                                                                                       \
            return result;                                                                                             \
        }                                                                                                              \
    } while (0)

#define PYFOLIO_TRY_ASSIGN(var, expr)                                                                                  \
    do {                                                                                                               \
        auto result = (expr);                                                                                          \
        if (result.is_error()) {                                                                                       \
            return result;                                                                                             \
        }                                                                                                              \
        var = result.value();                                                                                          \
    } while (0)

// Helper functions
inline std::string error_code_to_string(ErrorCode code) {
    switch (code) {
        case ErrorCode::Success:
            return "Success";
        case ErrorCode::InvalidInput:
            return "Invalid Input";
        case ErrorCode::InsufficientData:
            return "Insufficient Data";
        case ErrorCode::DivisionByZero:
            return "Division By Zero";
        case ErrorCode::NumericOverflow:
            return "Numeric Overflow";
        case ErrorCode::NumericUnderflow:
            return "Numeric Underflow";
        case ErrorCode::MissingData:
            return "Missing Data";
        case ErrorCode::InvalidDateRange:
            return "Invalid Date Range";
        case ErrorCode::InvalidSymbol:
            return "Invalid Symbol";
        case ErrorCode::InvalidCurrency:
            return "Invalid Currency";
        case ErrorCode::CalculationError:
            return "Calculation Error";
        case ErrorCode::FileNotFound:
            return "File Not Found";
        case ErrorCode::ParseError:
            return "Parse Error";
        case ErrorCode::NetworkError:
            return "Network Error";
        case ErrorCode::MemoryError:
            return "Memory Error";
        case ErrorCode::UnknownError:
            return "Unknown Error";
        default:
            return "Unknown Error Code";
    }
}

}  // namespace pyfolio