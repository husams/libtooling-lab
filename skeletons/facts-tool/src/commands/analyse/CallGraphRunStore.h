#pragma once

#include "commands/analyse/CallGraphRunRecord.h"

#include <cstdint>
#include <expected>
#include <string>

namespace facts::commands {

// Appends one run and all of its child rows to the facts store in a single
// transaction, migrating the store to schema version 12 first. A failed
// commit leaves no run behind and reports the SQLite text.
std::expected<std::int64_t, std::string>
persistCallGraphRun(const CallGraphRunRecord &record);

} // namespace facts::commands
