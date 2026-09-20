#include "apis/v2/symbols/Internal.h"
#include <charconv>
#include <set>

namespace facts::apis::v2::symbols {
domain::Error indexError(const std::string &message) {
  const bool cursor = message.find("cursor") != std::string::npos;
  return {cursor ? 409U : 503U, cursor ? "invalid_cursor" : "index_unavailable", message};
}
domain::Result<PageOptions> pageOptions(const QueryParameters &values) {
  PageOptions options;
  if (auto found = values.find("limit"); found != values.end()) {
    const auto &text = found->second;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), options.limit);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() ||
        options.limit < 1 || options.limit > 500)
      return std::unexpected(domain::Error{422, "invalid_query", "limit must be between 1 and 500"});
  }
  if (auto found = values.find("cursor"); found != values.end()) options.cursor = found->second;
  return options;
}
namespace {
domain::Result<index::Query> parse(const QueryParameters &values) {
  const std::set<std::string> allowed{"qualified_name", "kind", "usr", "repository",
      "repo", "component", "limit", "cursor", "match"};
  for (const auto &[name, value] : values)
    if (!allowed.contains(name) || value.empty() || value.find('\0') != std::string::npos ||
        value.size() > (name == "cursor" ? 256U : 4096U))
      return std::unexpected(domain::Error{422, "invalid_query", "Unknown, empty or oversized query parameter"});
  if (!values.contains("qualified_name") && !values.contains("usr"))
    return std::unexpected(domain::Error{422, "invalid_query", "qualified_name or usr is required"});
  if (values.contains("repo") && values.contains("repository"))
    return std::unexpected(domain::Error{422, "invalid_query", "Use only repository, not both repository and repo"});
  if (auto mode = values.find("match"); mode != values.end() && mode->second != "prefix" && mode->second != "exact")
    return std::unexpected(domain::Error{422, "invalid_query", "match must be prefix or exact"});
  return pageOptions(values).transform([&](const PageOptions &options) {
    index::Query query;
    query.match = values.contains("match") && values.at("match") == "exact"
        ? index::NameMatch::Exact : index::NameMatch::Prefix;
    query.distinct = true;
    query.limit = options.limit;
    query.cursor = options.cursor;
    if (values.contains("qualified_name")) query.qualifiedName = values.at("qualified_name");
    for (const auto &[key, target] : {std::pair{"kind", &query.kind}, {"usr", &query.usr},
         {"repo", &query.repository}, {"repository", &query.repository}, {"component", &query.component}})
      if (values.contains(key)) *target = values.at(key);
    return query;
  });
}
}
Json encode(const index::Symbol &symbol) {
  Json definition = nullptr;
  if (symbol.definition) definition = {{"file_id", std::to_string(symbol.fileId)},
      {"path", symbol.path}, {"line", nullptr}, {"column", nullptr}};
  return {{"symbol_id", symbol.symbolId}, {"qualified_name", symbol.qualifiedName},
      {"kind", symbol.kind}, {"usr", symbol.usr}, {"repository", symbol.repository},
      {"component", symbol.component}, {"definition", std::move(definition)}};
}
domain::Result<Json> search(const domain::Context &context, const QueryParameters &values) {
  return parse(values).and_then([&](const index::Query &query) {
    return index::search(context.configuration.database, query).transform_error(indexError);
  }).transform([](const index::Page &page) {
    Json items = Json::array();
    for (const auto &symbol : page.items) items.push_back(encode(symbol));
    return Json{{"items", std::move(items)}, {"next_cursor", page.nextCursor ? Json(*page.nextCursor) : Json(nullptr)},
        {"index_revision", std::to_string(page.generation)}};
  });
}
domain::Result<StoredSymbol> find(const domain::Context &context, std::string_view id) {
  if (id.size() != 68 || !id.starts_with("sym_") ||
      id.substr(4).find_first_not_of("0123456789abcdef") != std::string_view::npos)
    return std::unexpected(domain::Error{404, "symbol_not_found", "Unknown symbol"});
  index::Query query;
  query.symbolId = std::string(id);
  query.distinct = true;
  return index::search(context.configuration.database, query).transform_error(indexError)
      .and_then([](index::Page page) -> domain::Result<StoredSymbol> {
        if (page.items.empty()) return std::unexpected(domain::Error{404, "symbol_not_found", "Unknown symbol"});
        return StoredSymbol{std::move(page.items.front()), page.generation};
      });
}
domain::Result<Json> read(const domain::Context &context, std::string_view id,
                          std::string_view child, const QueryParameters &values) {
  return find(context, id).and_then([&](const StoredSymbol &symbol) -> domain::Result<Json> {
    if (child.empty()) {
      if (!values.empty()) return std::unexpected(domain::Error{422, "invalid_query", "Symbol detail has no query parameters"});
      return encode(symbol.symbol);
    }
    if (child != "occurrences" && child != "relations")
      return std::unexpected(domain::Error{404, "resource_not_found", "Unknown symbol resource"});
    return children(context, symbol, child, values);
  });
}
}
