#include "apis/v2/JobServices.h"

namespace facts::apis::v2::jobs {
Result<index::Symbol> resolveSymbol(const domain::Context &context, const Json &selector) {
  index::Query query;
  query.distinct = true;
  query.limit = 2;
  query.match = index::NameMatch::Exact;
  if (selector.contains("qualified_name")) query.qualifiedName = selector["qualified_name"].get<std::string>();
  if (selector.contains("usr")) query.usr = selector["usr"].get<std::string>();
  if (selector.contains("symbol_id")) query.symbolId = selector["symbol_id"].get<std::string>();
  if (selector.contains("repository")) query.repository = selector["repository"].get<std::string>();
  return index::search(context.configuration.database, query).transform_error(failed)
      .and_then([](index::Page page) -> Result<index::Symbol> {
        if (page.items.empty()) return std::unexpected(domain::Error{
            404, "symbol_not_found", "No indexed symbol matches the selector"});
        if (page.items.size() != 1 || page.nextCursor)
          return std::unexpected(domain::Error{409, "ambiguous_symbol",
              "Select one symbol using its symbol_id, USR, or repository"});
        return std::move(page.items.front());
      });
}
}
