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
Result<bool> hasStateCounts(Database &database);
Result<void> prepareStage(Database &database);
Result<void> prepareOwners(Database &database, const std::filesystem::path &base);
Result<std::vector<std::filesystem::path>> sources(
    Database &database, const std::filesystem::path &project);
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
inline constexpr std::string_view stageInsert =
    "INSERT INTO temp.api_symbol_stage(usr,qualified_name,file_id,kind,"
    "is_definition,path) SELECT ?1,?2,?3,?4,?5,?6 WHERE NOT EXISTS "
    "(SELECT 1 FROM temp.api_symbol_owner WHERE file_id=?3 AND facts_db<>?7) "
    "ON CONFLICT(usr,file_id) DO UPDATE SET "
    "qualified_name=excluded.qualified_name,kind=excluded.kind,"
    "path=CASE WHEN excluded.is_definition>=is_definition THEN excluded.path "
    "ELSE path END,is_definition=max(is_definition,excluded.is_definition)";
}
