#include "apis/watch/Scan.h"
#include "apis/watch/Paths.h"
#include "apis/watch/ignore/Ignore.h"
#include "apis/watch/plan/Plan.h"
#include "apis/watch/catalog/Ownership.h"

namespace facts::apis::watch {
namespace {
std::expected<void, std::string> traverse(Scan &scan, const Clone &clone,
    const Settings &settings, const std::atomic_bool &cancelled) {
  std::error_code error;
  if (!std::filesystem::is_directory(clone.path, error)) {
    scan.notices.push_back("clone directory unavailable: " + clone.path.string());
    return {};
  }
  auto rules = Ignore::create(clone.path, settings);
  if (!rules) return std::unexpected(rules.error());
  auto excluded = rules->excludes(clone.path, true);
  if (!excluded) return std::unexpected(excluded.error());
  if (*excluded) return {};
  scan.roots.insert(clone.path);
  scan.directories.insert(clone.path);
  for (const auto &file : rules->controlFiles()) {
    scan.controlFiles.insert(file);
    if (std::filesystem::is_directory(file.parent_path(), error))
      scan.directories.insert(file.parent_path());
    error.clear();
  }
  auto iterator = std::filesystem::recursive_directory_iterator(clone.path, error);
  const auto end = std::filesystem::recursive_directory_iterator{};
  while (!error && iterator != end) {
    if (cancelled) return std::unexpected("watch scan cancelled");
    const auto &entry = *iterator;
    const auto *selected = owner(entry.path(), scan.catalog);
    if (!selected || selected->cloneId != clone.cloneId) {
      iterator.disable_recursion_pending();
      iterator.increment(error);
      continue;
    }
    const bool directory = entry.is_directory(error);
    if (error) break;
    auto ignored = rules->excludes(entry.path(), directory);
    if (!ignored) return std::unexpected(ignored.error());
    if (*ignored || watch::ignored(entry.path(), settings)) {
      iterator.disable_recursion_pending();
    } else if (entry.is_symlink(error)) {
      iterator.disable_recursion_pending();
      if (!directory && entry.path().filename() == "compile_commands.json")
        scan.databases.insert(entry.path().parent_path());
    } else if (directory) {
      scan.directories.insert(entry.path());
    } else if (entry.path().filename() == "compile_commands.json") {
      scan.databases.insert(entry.path().parent_path());
    }
    iterator.increment(error);
  }
  if (error) return std::unexpected("cannot scan " + clone.path.string() + ": " +
                                    error.message());
  return {};
}
}
std::expected<Scan, std::string> discover(const Settings &settings, Catalog catalog,
                                         const std::atomic_bool &cancelled) {
  Scan scan;
  scan.catalog = std::move(catalog);
  for (const auto &clone : scan.catalog.clones) {
    if (!clone.active || !clone.excluded.empty()) continue;
    auto result = traverse(scan, clone, settings, cancelled);
    if (!result) return std::unexpected(result.error());
  }
  auto explicitPaths = explicitDatabases(settings);
  if (!explicitPaths) return std::unexpected(explicitPaths.error());
  for (const auto &directory : *explicitPaths) {
    std::error_code error;
    scan.controlFiles.insert(directory / "compile_commands.json");
    if (std::filesystem::is_directory(directory, error))
      scan.directories.insert(directory);
    else scan.notices.push_back("compilation directory unavailable: " + directory.string());
  }
  return scan;
}
}
