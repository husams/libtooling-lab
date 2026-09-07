#pragma once

#include "commands/analyse/CallGraphRunRecord.h"
#include "storage/SqliteDatabase.h"

#include <cstdint>
#include <expected>
#include <string>

namespace facts::commands {

std::expected<std::int64_t, std::string>
insertCallGraphRun(storage::Database &database,
                   const CallGraphRunRecord &record);

std::expected<void, std::string>
insertCallGraphRunRows(storage::Database &database, std::int64_t runId,
                       const CallGraphRunRecord &record);

} // namespace facts::commands
