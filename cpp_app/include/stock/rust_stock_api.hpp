#pragma once

#include "stock/stock_api.hpp"

namespace stock {

/// Adapter from the C++ `IStockApi` interface to the cxx.rs bridge exposed by
/// `rust_lib`. The heavy lifting (HTTP + JSON) stays in Rust.
class RustStockApi final : public IStockApi {
public:
    QuoteResult quote(const std::string& symbol) override;
};

} // namespace stock
