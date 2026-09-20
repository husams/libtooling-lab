#include "apis/v2/catalog/Internal.h"
#include <charconv>

namespace facts::apis::v2::catalog {
Result<void> fields(const Json &value,
                    std::initializer_list<std::string_view> allowed) {
  if (!value.is_object()) return std::unexpected(invalid("Expected a JSON object"));
  for (auto item = value.begin(); item != value.end(); ++item)
    if (std::find(allowed.begin(), allowed.end(), item.key()) == allowed.end())
      return std::unexpected(invalid("Unknown field: " + item.key()));
  return {};
}
Result<std::int64_t> identifier(std::string_view value) {
  std::int64_t result = 0;
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || result < 1)
    return std::unexpected(invalid("Resource ID must be a positive decimal string"));
  return result;
}
Result<std::string> text(const Json &value, std::string_view name,
                        bool required, bool nullable) {
  const std::string key(name);
  if (!value.contains(key)) {
    if (required) return std::unexpected(invalid("Missing field: " + key));
    return std::string{};
  }
  const auto &field = value.at(key);
  if (nullable && field.is_null()) return std::string{};
  if (!field.is_string() || (required && field.get_ref<const std::string &>().empty()))
    return std::unexpected(invalid(key + " must be " + (required ? "a nonempty" : "a") + " string"));
  if (field.get_ref<const std::string &>().size() > 4096)
    return std::unexpected(invalid(key + " exceeds 4096 bytes"));
  if (field.get_ref<const std::string &>().find('\0') != std::string::npos)
    return std::unexpected(invalid(key + " must not contain NUL characters"));
  return field.get<std::string>();
}
Result<Json> page(Json items, const Json &query) {
  unsigned limit = 50;
  if (query.contains("limit")) {
    const auto input = query.at("limit").get<std::string>();
    const auto parsed = std::from_chars(input.data(), input.data() + input.size(), limit);
    if (parsed.ec != std::errc{} || parsed.ptr != input.data() + input.size() || limit < 1 || limit > 500)
      return std::unexpected(invalid("limit must be between 1 and 500"));
  }
  std::int64_t after = 0;
  if (query.contains("cursor")) {
    const auto input = query.at("cursor").get<std::string>();
    if (!input.starts_with("after:")) return std::unexpected(invalid("Invalid cursor"));
    auto parsed = identifier(std::string_view(input).substr(6));
    if (!parsed) return std::unexpected(parsed.error());
    after = *parsed;
  }
  Json selected = Json::array();
  bool more = false;
  for (auto &item : items) {
    auto id = identifier(item.at("id").get<std::string>());
    if (!id) return std::unexpected(id.error());
    if (*id <= after) continue;
    if (selected.size() == limit) { more = true; break; }
    selected.push_back(std::move(item));
  }
  Json cursor = more ? Json("after:" + selected.back().at("id").get<std::string>()) : Json(nullptr);
  return Json{{"items", std::move(selected)}, {"next_cursor", std::move(cursor)}};
}
Result<native::Repository> repository(Database &database, std::int64_t id) {
  return lift(native::repositories(database)).and_then([&](auto values) -> Result<native::Repository> {
    std::erase_if(values, [&](const auto &value) { return value.id != id; });
    return lift(native::requireOne(std::move(values), "repository"));
  });
}
Result<native::File> file(Database &database, std::int64_t id) {
  return lift(native::files(database)).and_then([&](auto values) -> Result<native::File> {
    std::erase_if(values, [&](const auto &value) { return value.id != id; });
    return lift(native::requireOne(std::move(values), "file"));
  });
}
}
