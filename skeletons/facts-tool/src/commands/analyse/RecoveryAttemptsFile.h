#pragma once

#include "commands/analyse/RecoveryAttempts.h"

namespace facts::commands::recovery {
std::expected<InputFileIdentity, AttemptError>
inspectInput(const std::string &path);
std::expected<std::string, AttemptError> hashInput(const std::string &path);
} // namespace facts::commands::recovery
