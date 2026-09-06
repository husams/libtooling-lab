#pragma once

#include <expected>
#include <string>
#include <vector>

namespace facts::config { struct Resolved; }

namespace facts::commands {

std::expected<void, std::string>
invalidateCallGraphEntriesBeforeMutation(const std::string &configuration,
                                         const std::string &facts);

std::expected<void, std::string> invalidateConfiguredCallGraphEntries(
    const config::Resolved &resolved, const std::string &explicitFacts,
    const std::vector<std::string> &sources = {});

} // namespace facts::commands
