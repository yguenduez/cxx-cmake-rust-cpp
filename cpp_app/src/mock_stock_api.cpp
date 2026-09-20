#include "stock/mock_stock_api.hpp"

#include <cctype>
#include <cstdint>
#include <string>

namespace stock {
namespace {

std::string normalize(std::string symbol) {
    for (char& c : symbol) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return symbol;
}

} // namespace

QuoteResult MockStockApi::quote(const std::string& symbol) {
    const std::string sym = normalize(symbol);
    if (sym.empty()) {
        return {std::nullopt, "Enter a stock symbol."};
    }
    if (sym == "ERR" || sym == "FAIL") {
        return {std::nullopt, "Unknown symbol: " + sym};
    }

    // Deterministic pseudo price in the range 10.00 - 999.99.
    std::uint64_t hash = 1469598103934665603ull;
    for (char c : sym) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ull;
    }

    const double base = 10.0 + static_cast<double>(hash % 99000ull) / 100.0;
    const double change = (static_cast<double>((hash >> 17) % 400ull) - 200.0) / 100.0;

    Quote quote;
    quote.current = base;
    quote.change = change;
    quote.percent_change = change / base * 100.0;
    quote.high = base + 1.5;
    quote.low = base - 1.5;
    quote.open = base - change;
    quote.previous_close = base - change;
    quote.timestamp = 1700000000;

    return {quote, {}};
}

} // namespace stock
