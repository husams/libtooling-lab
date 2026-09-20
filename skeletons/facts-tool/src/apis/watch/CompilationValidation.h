#pragma once
#include <expected>
#include <filesystem>
#include <set>
#include <string>

namespace facts::apis::watch {
// Reject cross-database conflicts before either manual or automatic import
// mutates stored compilation commands. Paths name containing directories.
std::expected<void, std::string> validateCompilationDatabases(
    const std::set<std::filesystem::path> &directories);
}
