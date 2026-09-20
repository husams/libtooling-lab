#pragma once
#include "apis/v2/symbols/Symbols.h"
#include "apis/index/Index.h"
#include "storage/catalog/Database.h"

namespace facts::apis::v2::symbols {
struct PageOptions { unsigned limit = 50; std::optional<std::string> cursor; };
struct StoredSymbol { index::Symbol symbol; std::int64_t generation = 0; };
domain::Error indexError(const std::string &);
domain::Result<PageOptions> pageOptions(const QueryParameters &);
domain::Result<StoredSymbol> find(const domain::Context &, std::string_view id);
Json encode(const index::Symbol &);
domain::Result<Json> children(const domain::Context &, const StoredSymbol &,
                              std::string_view child, const QueryParameters &);
}
