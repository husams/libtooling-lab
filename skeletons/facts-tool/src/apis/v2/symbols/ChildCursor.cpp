#include "apis/v2/symbols/Children.h"
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA256.h>

namespace facts::apis::v2::symbols {
namespace {
std::string hex(const std::string &value) {
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  for (unsigned char byte : value) { result += digits[byte >> 4]; result += digits[byte & 15]; }
  return result;
}
std::optional<std::string> unhex(const std::string &value) {
  if (value.empty() || value.size() % 2) return std::nullopt;
  std::string result;
  for (std::size_t i = 0; i < value.size(); i += 2) {
    const auto high = std::string_view("0123456789abcdef").find(value[i]);
    const auto low = std::string_view("0123456789abcdef").find(value[i + 1]);
    if (high == std::string_view::npos || low == std::string_view::npos) return std::nullopt;
    result += static_cast<char>((high << 4) | low);
  }
  return result;
}
}
std::string digest(const std::string &value) {
  llvm::SHA256 hash;
  hash.update(value);
  const auto bytes = hash.final();
  return hex(std::string(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
}
std::string fingerprint(const SourceCatalog &catalog, std::int64_t generation) {
  Json identity = Json::array({generation});
  for (const auto &path : catalog.sources) {
    for (const auto &file : {path, std::filesystem::path(path.string() + "-wal")}) {
      std::error_code error;
      const auto size = std::filesystem::file_size(file, error);
      if (error) { identity.push_back(Json::array({file.string(), "missing"})); continue; }
      const auto time = std::filesystem::last_write_time(file, error);
      identity.push_back(Json::array({file.string(), size, error ? 0 : time.time_since_epoch().count()}));
    }
  }
  return digest(identity.dump());
}
domain::Result<ChildPage> childPage(const QueryParameters &parameters, const StoredSymbol &symbol,
                                   std::string_view child, const SourceCatalog &sources) {
  return pageOptions(parameters).and_then([&](const PageOptions &options) -> domain::Result<ChildPage> {
    auto scope = parameters;
    scope.erase("limit"); scope.erase("cursor");
    ChildPage page{options.limit, "", digest(Json::array({symbol.symbol.symbolId, child, scope}).dump()),
                   fingerprint(sources, symbol.generation), {}};
    if (!options.cursor) return page;
    const auto decoded = unhex(*options.cursor);
    if (decoded) {
      const auto cursor = Json::parse(*decoded, nullptr, false);
      if (cursor.is_array() && cursor.size() == 3 && cursor[0] == page.identity &&
          cursor[1] == page.fingerprint && cursor[2].is_string()) {
        page.after = cursor[2].get<std::string>();
        if (page.after.size() == 64 && page.after.find_first_not_of("0123456789abcdef") == std::string::npos)
          return page;
      }
    }
    return std::unexpected(domain::Error{409, "invalid_cursor", "Cursor expired or does not match the symbol query"});
  });
}
Json finish(ChildPage page, std::int64_t generation) {
  const bool more = page.items.size() > page.limit;
  if (more) page.items.erase(std::prev(page.items.end()));
  Json items = Json::array();
  for (auto &[key, item] : page.items) items.push_back(std::move(item));
  Json cursor = nullptr;
  if (more) cursor = hex(Json::array({page.identity, page.fingerprint, page.items.rbegin()->first}).dump());
  return {{"items", std::move(items)}, {"next_cursor", std::move(cursor)},
          {"index_revision", std::to_string(generation)}};
}
}
