#include "storage/driverprobe/Database.h"

#include "storage/astcache/Connection.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA256.h>

#include <algorithm>
#include <ranges>

namespace facts::storage::driverprobe {
namespace {

using astcache::detail::Result;

constexpr std::string_view schema = R"sql(
CREATE TABLE IF NOT EXISTS driver_include_probe (
  key TEXT PRIMARY KEY NOT NULL,
  include_count INTEGER NOT NULL CHECK(include_count > 0),
  digest TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS driver_include_path (
  probe_key TEXT NOT NULL REFERENCES driver_include_probe(key) ON DELETE CASCADE,
  position INTEGER NOT NULL CHECK(position >= 0),
  path TEXT NOT NULL CHECK(length(path) > 0),
  PRIMARY KEY(probe_key, position)
);
)sql";

std::string digest(const IncludePaths &includes) {
  llvm::SHA256 hash;
  for (const auto &path : includes) {
    const auto text = path.string();
    hash.update(std::to_string(text.size()) + ":");
    hash.update(text);
  }
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  for (const auto byte : hash.final()) {
    result += digits[byte >> 4U];
    result += digits[byte & 15U];
  }
  return result;
}

struct Header {
  std::int64_t count;
  std::string digest;
};

Result<std::optional<IncludePaths>> load(storage::Database &database,
                                         std::string_view key) {
  return catalog::query(database,
      "SELECT include_count,digest FROM driver_include_probe WHERE key=?",
      [](const Row &row) { return Header{row.integer(0), row.string(1)}; },
      std::string(key))
      .and_then([&](auto headers) -> Result<std::optional<IncludePaths>> {
        if (headers.empty())
          return std::optional<IncludePaths>{};
        const auto &header = headers.front();
        return catalog::query(database,
            "SELECT position,path FROM driver_include_path WHERE probe_key=? ORDER BY position",
            [](const Row &row) { return std::pair{row.integer(0), row.string(1)}; },
            std::string(key))
            .transform([&](auto rows) -> std::optional<IncludePaths> {
              if (rows.empty() || static_cast<std::int64_t>(rows.size()) != header.count)
                return std::nullopt;
              IncludePaths includes;
              for (const auto &[position, path] : rows) {
                if (position != static_cast<std::int64_t>(includes.size()) || path.empty())
                  return std::nullopt;
                includes.emplace_back(path);
              }
              if (digest(includes) != header.digest)
                return std::nullopt;
              return includes;
            });
      });
}

Result<void> store(storage::Database &database, std::string_view key,
                    const IncludePaths &includes) {
  return database.executeScript(schema)
      .transform_error([&](auto) { return catalog::databaseError(database); })
      .and_then([&] {
        return catalog::execute(database, "DELETE FROM driver_include_probe WHERE key=?",
                                std::string(key));
      })
      .and_then([&] {
        return catalog::execute(database,
            "INSERT INTO driver_include_probe(key,include_count,digest) VALUES(?,?,?)",
            std::string(key), static_cast<std::int64_t>(includes.size()), digest(includes));
      })
      .and_then([&] {
        return database.executeBulk(
            "INSERT INTO driver_include_path(probe_key,position,path) VALUES(?,?,?)",
            std::views::iota(std::size_t{0}, includes.size()),
            [&](sqlite3_stmt *statement, std::size_t index) {
              return bindParameters(statement, std::string(key),
                                    static_cast<std::int64_t>(index), includes[index].string());
            }, {.atomic = false})
            .transform_error([&](auto) { return catalog::databaseError(database); })
            .transform([](auto) {});
      });
}

} // namespace

Result<std::optional<IncludePaths>> read(const std::filesystem::path &project,
                                         std::string_view key) {
  return astcache::detail::open(project, false).and_then([&](storage::Database database) {
    return astcache::detail::transact(database, false, [&] {
      return catalog::query(database,
          "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND "
          "name IN ('driver_include_probe','driver_include_path')",
          [](const Row &row) { return row.integer(0); })
          .and_then([&](auto counts) -> Result<std::optional<IncludePaths>> {
            if (counts.empty() || counts.front() != 2)
              return std::optional<IncludePaths>{};
            return load(database, key);
          });
    });
  });
}

Result<void> write(const std::filesystem::path &project, std::string_view key,
                   const IncludePaths &includes) {
  if (includes.empty() || std::ranges::any_of(includes, [](const auto &path) { return path.empty(); }))
    return std::unexpected("GNU include probe cannot cache an empty search result");
  return astcache::detail::open(project, true).and_then([&](storage::Database database) {
    return astcache::detail::transact(database, true, [&] { return store(database, key, includes); });
  });
}

} // namespace facts::storage::driverprobe
