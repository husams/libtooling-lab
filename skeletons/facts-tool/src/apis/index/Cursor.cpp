#include "apis/index/Internal.h"
#include <nlohmann/json.hpp>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA256.h>

namespace facts::apis::index {
namespace {
std::string identity(const Query &query) {
  const auto value = nlohmann::json::array({query.qualifiedName, query.kind.value_or(""),
      query.usr.value_or(""), query.repository.value_or(""), query.component.value_or("")});
  llvm::SHA256 hash;
  hash.update(value.dump());
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  for (const auto byte : hash.final()) {
    result += digits[byte >> 4];
    result += digits[byte & 15];
  }
  return result;
}
int digit(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  return -1;
}
}
std::string encodeCursor(const Query &query, const Position &position) {
  const auto value = nlohmann::json::array(
      {position.generation, position.record, identity(query)}).dump();
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(value.size() * 2);
  for (unsigned char byte : value) {
    result += digits[byte >> 4];
    result += digits[byte & 15];
  }
  return result;
}
Result<Position> decodeCursor(const Query &query) {
  if (!query.cursor) return Position{};
  const auto &input = *query.cursor;
  if (input.empty() || input.size() % 2 != 0 || input.size() > 256)
    return std::unexpected("invalid symbol cursor");
  std::string decoded;
  for (std::size_t offset = 0; offset < input.size(); offset += 2) {
    const int high = digit(input[offset]), low = digit(input[offset + 1]);
    if (high < 0 || low < 0) return std::unexpected("invalid symbol cursor");
    decoded += static_cast<char>((high << 4) | low);
  }
  try {
    const auto value = nlohmann::json::parse(decoded);
    if (!value.is_array() || value.size() != 3 || value[2] != identity(query))
      return std::unexpected("symbol cursor does not match the query");
    Position result{value[0].get<std::int64_t>(), value[1].get<std::int64_t>()};
    if (result.generation <= 0 || result.record <= 0)
      return std::unexpected("invalid symbol cursor");
    return result;
  } catch (const nlohmann::json::exception &) {
    return std::unexpected("invalid symbol cursor");
  }
}
}
