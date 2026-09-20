#include "apis/v2/http/Dispatch.h"
#include <charconv>
#include <limits>

namespace facts::apis::v2::http {
namespace {
domain::Result<std::size_t> number(std::string_view text) {
  std::size_t value = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
    return std::unexpected(domain::Error{400, "invalid_query", "Expected an unsigned integer"});
  return value;
}
}
domain::Result<Json> page(const Json &items, const Json &query, std::string_view identity) {
  std::size_t offset = 0, limit = 50;
  if (query.contains("limit")) {
    const auto parsed = number(query["limit"].get<std::string>());
    if (!parsed || *parsed == 0 || *parsed > 500)
      return std::unexpected(domain::Error{400, "invalid_query", "limit must be between 1 and 500"});
    limit = *parsed;
  }
  const auto prefix = std::string(identity) + ":";
  if (query.contains("cursor")) {
    const auto cursor = query["cursor"].get<std::string>();
    if (!cursor.starts_with(prefix))
      return std::unexpected(domain::Error{409, "invalid_cursor", "Cursor does not belong to this collection"});
    const auto parsed = number(std::string_view(cursor).substr(prefix.size()));
    if (!parsed || *parsed > items.size())
      return std::unexpected(domain::Error{409, "invalid_cursor", "Invalid result cursor"});
    offset = *parsed;
  }
  Json result = Json::array();
  const auto end = std::min(items.size(), offset + limit);
  for (auto index = offset; index < end; ++index) result.push_back(items[index]);
  return Json{{"items", result}, {"next_cursor", end < items.size() ? Json(prefix + std::to_string(end)) : Json(nullptr)}};
}
}
