#pragma once
#include "apis/index/Index.h"
#include "storage/catalog/Database.h"
#include <string_view>

namespace facts::apis::index {
using Database = catalog::Database;
struct Position { std::int64_t generation = 0, record = 0; };
Result<void> initialize(Database &database);
Result<void> ensureStateCounts(Database &database);
Result<void> ensurePositions(Database &database);
Result<void> ensureIdentities(Database &database);
Result<void> updateIdentities(Database &database);
Result<bool> hasStateCounts(Database &database);
Result<void> prepareStage(Database &database);
Result<void> prepareOwners(Database &database, const std::filesystem::path &base);
Result<std::vector<std::filesystem::path>> sources(
    Database &database, const std::filesystem::path &project);
Result<std::string> sourceFingerprint(const std::filesystem::path &path);
Result<void> stageCachedSources(Database &database);
Result<bool> copySource(Database &database, const std::filesystem::path &path);
Result<RefreshResult> publish(Database &database, std::size_t sources,
                              std::size_t missing);
Result<void> validate(const Query &query);
Result<Position> decodeCursor(const Query &query);
std::string encodeCursor(const Query &query, const Position &position);
Result<Page> readPage(Database &database, const Query &query,
                      const Position &position);
Result<std::string> kindName(std::int64_t stored);
Result<void> stage(Database &database, storage::Statement &statement,
                   const std::string &usr, const std::string &name,
                   std::int64_t file, std::int64_t kind, bool definition,
                   const std::string &path, const std::string &source);
}
