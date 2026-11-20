#include "stock/rust_stock_api.hpp"

#include "rust/cxx.h"
#include "rust_lib/src/lib.rs.h"

#include <exception>

namespace stock {

QuoteResult RustStockApi::quote(const std::string& symbol) {
    try {
        const rust_lib::Quote quote = rust_lib::stock_quote(rust::Str(symbol));

        Quote result;
        result.current = quote.current;
        if (quote.has_change) {
            result.change = quote.change;
            result.percent_change = quote.percent_change;
        }
        result.high = quote.high;
        result.low = quote.low;
        result.open = quote.open;
        result.previous_close = quote.previous_close;
        result.timestamp = quote.timestamp;
        return result;
    } catch (const std::exception& error) {
        // Covers rust::Error from Rust and std::invalid_argument thrown by
        // rust::Str for input that is not valid UTF-8.
        return std::string(error.what());
    }
}

} // namespace stock
