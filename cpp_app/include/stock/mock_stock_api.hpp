#pragma once

#include "stock/stock_api.hpp"

namespace stock {

/// Offline test double for `IStockApi`, selected with `cpp_app --mock`.
/// Returns deterministic fake prices derived from the symbol, so the UI works
/// without network access or an API key.
class MockStockApi final : public IStockApi {
public:
    QuoteResult quote(const std::string& symbol) override;
};

} // namespace stock
