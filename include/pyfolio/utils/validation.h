#pragma once

#include "../core/error_handling.h"
#include "../core/types.h"

namespace pyfolio::utils {

/**
 * @brief Validate return series
 */
Result<void> validate_returns(const ReturnSeries& returns) {
    if (returns.empty()) {
        return Result<void>::error(ErrorCode::InsufficientData, "Return series is empty");
    }

    return Result<void>::success();
}

/**
 * @brief Validate price series
 */
Result<void> validate_prices(const PriceSeries& prices) {
    if (prices.empty()) {
        return Result<void>::error(ErrorCode::InsufficientData, "Price series is empty");
    }

    for (const auto& price : prices) {
        if (price <= 0.0) {
            return Result<void>::error(ErrorCode::InvalidInput, "All prices must be positive");
        }
    }

    return Result<void>::success();
}

}  // namespace pyfolio::utils