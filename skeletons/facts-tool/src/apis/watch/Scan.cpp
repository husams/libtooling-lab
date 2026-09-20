#include "apis/watch/Scan.h"
#include "apis/watch/Snapshot.h"
#include "apis/watch/Paths.h"
#include "apis/watch/ignore/Ignore.h"
#include "apis/watch/plan/Plan.h"
#include "apis/watch/catalog/Ownership.h"
#include <algorithm>

namespace facts::apis::watch {
namespace {
using Path = std::filesystem::path;
bool inside(const Path &path, const Path &root) {
  const auto relative = path.lexically_relative(root);
  return !relative.empty() && !relative.is_absolute() && *relative.begin() != "..";
}
bool allowed(const Path &target, const Catalog &catalog) {
  const Clone *selected = nullptr;
  std::size_t rootLength = 0;
  for (const auto &clone : catalog.clones) {
    std::error_code error;
    const auto root = std::filesystem::canonical(clone.path, error);
    if (!error && inside(target, root) && root.native().size() > rootLength) {
      selected = &clone;
      rootLength = root.native().size();
    }
  }
  return selected && selected->active && selected->excluded.empty();
}

void warn(Scan &scan, const std::string &code, const Path &path,
          const std::string &message) {
  std::error_code error;
  auto target = std::filesystem::read_symlink(path, error);
  scan.warnings.push_back({code, path, error ? Path{} : target, message});
}
std::expected<void, std::string> traverse(Scan &scan, const Clone &clone,
    const Settings &settings, const std::atomic_bool &cancelled) {
  std::error_code error;
  const auto root = std::filesystem::canonical(clone.path, error);
  if (error || !std::filesystem::is_directory(root, error)) {
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
  // A stack of independent directory iterators lets a disappearing or unreadable
  // subtree be skipped without losing the rest of the repository scan.
  std::vector<Path> pending{clone.path};
  std::set<Path> visited{root};
  while (!pending.empty()) {
    const auto directory = std::move(pending.back());
    pending.pop_back();
    auto iterator = std::filesystem::directory_iterator(directory, error);
    const auto end = std::filesystem::directory_iterator{};
    if (error) {
      scan.directories.erase(directory);
      scan.notices.push_back("cannot scan " + directory.string() + ": " + error.message());
      warn(scan, "unreadable_directory", directory, error.message());
      error.clear();
      continue;
    }
    while (iterator != end) {
      if (cancelled) return std::unexpected("watch scan cancelled");
      const auto path = iterator->path();
      const auto *selected = owner(path, scan.catalog);
      if (selected && selected->cloneId == clone.cloneId && !watch::ignored(path, settings)) {
        const auto linkStatus = std::filesystem::symlink_status(path, error);
        const bool link = !error && std::filesystem::is_symlink(linkStatus);
        if (link) {
          scan.controlFiles.insert(path); // link replacement/deletion matters too
          std::error_code targetError;
          const auto raw = std::filesystem::read_symlink(path, targetError);
          scan.inputs[path] = "link:" + (targetError ? targetError.message() : raw.string());
        }
        const auto status = error ? linkStatus : std::filesystem::status(path, error);
        if (error || !std::filesystem::exists(status)) {
          const auto code = error == std::errc::too_many_symbolic_link_levels
              ? "symlink_cycle" : link ? "broken_symlink" : "unavailable_entry";
          warn(scan, code, path, error ? error.message() : "target does not exist");
          error.clear();
        } else {
          const bool isDirectory = std::filesystem::is_directory(status);
          auto ignored = rules->excludes(path, isDirectory);
          if (!ignored) return std::unexpected(ignored.error());
          if (!*ignored) {
            const auto target = std::filesystem::canonical(path, error);
            if (error) {
              warn(scan, link ? "broken_symlink" : "unavailable_entry", path, error.message());
              error.clear();
            } else if (link && !allowed(target, scan.catalog)) {
              warn(scan, "symlink_outside_roots", path, "target is outside active registered roots");
            } else if (isDirectory) {
              if (link) scan.inputs[path] += ";target:" + target.string();
              scan.directories.insert(path);
              scan.aliases.emplace(path, target);
              if (visited.insert(target).second) pending.push_back(path);
              else if (link) {
                const auto parentTarget = std::filesystem::canonical(path.parent_path(), error);
                if (!error && inside(parentTarget, target))
                  warn(scan, "symlink_cycle", path, "directory target is an ancestor");
                error.clear();
              }
            } else if (std::filesystem::is_regular_file(status)) {
              if (watch::relevant(path) || path.filename() == ".gitignore" || link) {
                auto content = fingerprint(path);
                if (content) scan.inputs[path] += ";target:" + target.string() + ";content:" + *content;
                else {
                  scan.notices.push_back(content.error());
                  warn(scan, "unavailable_entry", path, content.error());
                }
              }
              if (link) {
                // The target's extension need not match its C++/database alias.
                scan.controlFiles.insert(target);
              }
              if (path.filename() == "compile_commands.json")
                scan.databases.insert(path.parent_path());
            }
          }
        }
      }
      iterator.increment(error);
      if (error) {
        scan.notices.push_back("cannot finish scanning " + directory.string() + ": " + error.message());
        warn(scan, "unreadable_directory", directory, error.message());
        error.clear();
        break;
      }
    }
  }
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
  for (const auto &path : scan.controlFiles) {
    std::error_code error;
    if (scan.inputs.contains(path) || !std::filesystem::is_regular_file(path, error)) continue;
    auto content = fingerprint(path);
    if (content) scan.inputs[path] = *content;
    else scan.notices.push_back(content.error());
  }
  // Configuration content can change at the same path between server runs.
  for (std::size_t index = 0; index + 1 < settings.defaults.size(); index += 2) {
    if (settings.defaults[index] != "--config") continue;
    auto content = fingerprint(settings.defaults[index + 1]);
    if (content) scan.inputs[settings.defaults[index + 1]] = *content;
  }
  scan.signature = signature(settings, scan);
  return scan;
}
}
