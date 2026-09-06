#pragma once

#include <expected>
#include <string>
#include <vector>

namespace facts::config {
struct Resolved;
}

namespace facts::commands {

std::expected<bool, std::string>
callGraphProjectHasFiles(const std::string &configuration);

std::expected<std::vector<std::string>, std::string>
configuredCallGraphFactPaths(const config::Resolved &resolved,
                             const std::string &explicitFacts,
                             const std::vector<std::string> &sources);

std::expected<void, std::string>
invalidateCallGraphEntriesBeforeMutation(const std::string &configuration,
                                         const std::string &facts);

std::expected<void, std::string> invalidateConfiguredCallGraphEntries(
    const config::Resolved &resolved, const std::string &explicitFacts,
    const std::vector<std::string> &sources = {});

} // namespace facts::commands
