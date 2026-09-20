#pragma once
#include "apis/watch/plan/Plan.h"
#include "apis/watch/ignore/Ignore.h"
#include <set>

namespace facts::apis::watch::plan {
struct Options {
  std::vector<std::string> arguments;
  std::vector<std::filesystem::path> databases;
};
struct Compilation {
  std::filesystem::path directory;
  std::vector<std::string> sources;
};
std::expected<Options, std::string>
options(const Settings &settings, bool importing);
std::vector<std::string> arguments(const Settings &settings, const Catalog &catalog,
                                  const Options &options, bool importing);
std::expected<std::vector<Compilation>, std::string>
compilations(const std::vector<std::filesystem::path> &directories);
std::expected<std::vector<std::string>, std::string>
storedSources(const Catalog &catalog);
std::expected<std::vector<std::string>, std::string> selectSources(
    const std::vector<std::string> &sources, const Catalog &catalog,
    const Clone &clone, const Ignore &ignore, const Settings &settings);
std::expected<void, std::string> appendBatches(
    std::vector<std::vector<std::string>> &jobs,
    std::vector<std::string> arguments, const std::set<std::string> &sources);
}
