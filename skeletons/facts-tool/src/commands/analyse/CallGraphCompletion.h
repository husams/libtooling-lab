#pragma once

#include "commands/analyse/CallGraphRunRecord.h"

#include <cstdint>
#include <expected>
#include <string>

namespace facts::commands {

// Prints the completion line for a committed run and maps its status to the
// exit contract: complete/truncated succeed, every other status is reported
// through one stderr line by the dispatcher.
std::expected<int, std::string>
completeCallGraphRun(const CallGraphRunRecord &record, std::int64_t runId);

} // namespace facts::commands
