#pragma once

#include "commands/analyse/CallGraphRecoveryInternal.h"

#include <set>

namespace facts::commands {

std::vector<FileId> indexedFiles(const RecoveryContext &context,
                                 std::string_view usr);
std::vector<FileId> fallbackFiles(const RecoveryContext &context,
                                  FileId declarationFile);
RecoveryEntry makeEntry(const RecoveryContext &context, FileId id,
                        std::vector<std::string> usrs, std::string reason);

} // namespace facts::commands
