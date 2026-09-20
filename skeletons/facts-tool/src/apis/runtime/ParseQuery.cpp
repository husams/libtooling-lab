#include "apis/runtime/Validation.h"
#include <charconv>
#include <map>

namespace facts::apis::runtime {
domain::Result<index::Query> parseQuery(std::string_view target) {
  std::map<std::string, std::string> values;
  const auto start = target.find('?');
  auto query = start == std::string_view::npos ? std::string_view{} : target.substr(start + 1);
  const std::set<std::string> allowed{
      "qualified_name", "kind", "usr", "repo", "component", "limit", "cursor"};
  while (!query.empty()) {
    const auto end = query.find('&');
    const auto pair = query.substr(0, end);
    const auto equal = pair.find('=');
    auto key = decodeQuery(pair.substr(0, equal));
    auto value = decodeQuery(equal == std::string_view::npos ? "" : pair.substr(equal + 1));
    if (!key) return std::unexpected(key.error());
    if (!value) return std::unexpected(value.error());
    const auto maximum = *key == "cursor" ? 256U : 4096U;
    if (!allowed.contains(*key) || values.contains(*key) || value->empty() || value->size() > maximum)
      return std::unexpected(domain::Error{400, "invalid_query", "Unknown, duplicate, empty or oversized query parameter"});
    values.emplace(std::move(*key), std::move(*value));
    query = end == std::string_view::npos ? std::string_view{} : query.substr(end + 1);
  }
  if (!values.contains("qualified_name")) return std::unexpected(domain::Error{
      400, "invalid_query", "qualified_name is required"});
  index::Query result;
  result.qualifiedName = values.at("qualified_name");
  for (auto [name, field] : {std::pair{"kind", &result.kind}, {"usr", &result.usr},
      {"repo", &result.repository}, {"component", &result.component}, {"cursor", &result.cursor}})
    if (values.contains(name)) *field = values.at(name);
  if (values.contains("limit")) {
    const auto &value = values.at("limit");
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result.limit);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
        result.limit < 1 || result.limit > 500)
      return std::unexpected(domain::Error{400, "invalid_query", "limit must be an integer from 1 to 500"});
  }
  return result;
}
}
