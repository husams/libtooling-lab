#include "apis/runtime/Validation.h"
#include <algorithm>
#include <charconv>

namespace facts::apis::runtime {
domain::Result<void> keys(const Json &value, const std::set<std::string> &allowed) {
  if (!value.is_object())
    return std::unexpected(domain::Error{400, "invalid_request", "Expected a JSON object"});
  for (const auto &[key, item] : value.items()) if (!allowed.contains(key))
    return std::unexpected(domain::Error{400, "invalid_request", "Unknown field: " + key});
  return {};
}
domain::Result<std::string> text(const Json &value, std::string_view name,
                                std::size_t limit) {
  if (!value.is_string()) return std::unexpected(domain::Error{
      400, "invalid_request", std::string(name) + " must be a string"});
  auto result = value.get<std::string>();
  if (result.empty() || result.size() > limit ||
      std::ranges::any_of(result, [](unsigned char c) { return c == 0; }))
    return std::unexpected(domain::Error{400, "invalid_request",
        std::string(name) + " must be nonempty and within its size limit, without NUL"});
  return result;
}
domain::Result<std::string> decodeQuery(std::string_view value) {
  std::string result;
  for (std::size_t i = 0; i < value.size(); ++i) {
    auto character = value[i];
    if (character == '%') {
      unsigned number = 0;
      if (i + 2 >= value.size()) return std::unexpected(domain::Error{
          400, "invalid_query", "Incomplete percent encoding"});
      const auto parsed = std::from_chars(value.data() + i + 1,
                                         value.data() + i + 3, number, 16);
      if (parsed.ec != std::errc{} || parsed.ptr != value.data() + i + 3)
        return std::unexpected(domain::Error{400, "invalid_query", "Invalid percent encoding"});
      character = static_cast<char>(number);
      i += 2;
    } else if (character == '+') character = ' ';
    if (static_cast<unsigned char>(character) < 32 || character == 127)
      return std::unexpected(domain::Error{400, "invalid_query", "Control character in query"});
    result += character;
  }
  return result;
}
}
