#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace stock {

/// A real-time quote. Mirrors the `Quote` type exposed by `rust_lib`.
struct Quote {
    /// Current price.
    double current = 0.0;
    /// Change since previous close.
    std::optional<double> change;
    /// Percent change since previous close.
    std::optional<double> percent_change;
    /// High price of the day.
    double high = 0.0;
    /// Low price of the day.
    double low = 0.0;
    /// Open price of the day.
    double open = 0.0;
    /// Previous close price.
    double previous_close = 0.0;
    /// Unix timestamp of the quote.
    std::int64_t timestamp = 0;
};

/// Result of a quote lookup: either a quote or a human readable error.
struct QuoteResult {
    std::optional<Quote> quote;
    std::string error;

    [[nodiscard]] bool ok() const noexcept { return quote.has_value(); }
    explicit operator bool() const noexcept { return ok(); }
};

/// Abstraction over the stock price backend.
///
/// The UI only depends on this interface. Start with `MockStockApi`; later the
/// FFI wrapper around `rust_lib` implements the same interface and the UI does
/// not change.
class IStockApi {
public:
    virtual ~IStockApi() = default;

    /// Looks up the current quote for `symbol` (e.g. "AAPL").
    virtual QuoteResult quote(const std::string& symbol) = 0;

    /// Convenience wrapper returning only the current price.
    virtual std::optional<double> current_price(const std::string& symbol) {
        auto result = quote(symbol);
        if (!result.ok()) {
            return std::nullopt;
        }
        return result.quote->current;
    }
};

} // namespace stock
