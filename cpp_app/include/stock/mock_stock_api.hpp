#pragma once

#include "stock/stock_api.hpp"

namespace stock {

/// Placeholder stock API used until the real FFI wrapper around `rust_lib` is
/// wired up. Returns deterministic fake prices derived from the symbol, so the
/// UI can be developed and tested without network access.
class MockStockApi final : public IStockApi {
public:
    QuoteResult quote(const std::string& symbol) override;
};

} // namespace stock
