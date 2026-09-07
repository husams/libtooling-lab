#pragma once

#include "commands/analyse/CallGraphRecoveryInternal.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace facts::commands {
using RecoveryReuseGroups = std::map<FileId, std::set<std::string>>;

std::vector<RecoveryEntry> makeRecoveryReuseReport(const RecoveryContext &,
                                                   const RecoveryReuseGroups &);
} // namespace facts::commands
