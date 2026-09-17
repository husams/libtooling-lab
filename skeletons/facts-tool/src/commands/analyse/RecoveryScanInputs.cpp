#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "commands/analyse/RecoveryRegistryHash.h"
#include "commands/analyse/RecoveryScan.h"
#include "storage/catalog/File.h"
#include "tooling/FrontendActivity.h"
#include <clang/Basic/FileEntry.h>
#include <optional>

namespace facts::commands {
namespace {
using RegisteredInputs = std::map<std::string, recovery::RegisteredInput>;

RegisteredInputs::const_iterator
findRegisteredInput(const RegisteredInputs &registered,
                     const std::string &path) {
  if (const auto found = registered.find(path); found != registered.end())
    return found;
  // Snapshots also retain Clang's original file spelling. Resolve unmatched
  // aliases physically before removing parent components: alias/../header
  // can name a different file from its lexically normalized spelling.
  std::error_code error;
  const auto canonical = std::filesystem::canonical(path, error);
  return error ? registered.end() : registered.find(canonical.string());
}

std::optional<std::set<std::string>>
inputPaths(const RecoveryCandidate &candidate, const RecoveryScan &scan,
           int verbosity) {
  std::set<std::string> paths{candidate.source.lexically_normal().string()};
  // Cached ASTs already have their complete input closure in project.db.
  // Reuse it without materializing SourceManager file entries again. Cache
  // misses capture the same metadata once alongside the AST parse.
  if (scan.status == 0 && !scan.includes.visitedSources.empty()) {
    for (const auto &path : scan.includes.visitedSources)
      paths.insert(path);
    return paths;
  }
  for (const auto &unit : scan.units) {
    reportFrontendActivity(verbosity, "include-reconstruction",
                           candidate.source.string());
    const auto &manager = unit->getSourceManager();
    for (auto it = manager.fileinfo_begin(); it != manager.fileinfo_end();
         ++it) {
      const auto real = it->first.getFileEntry().tryGetRealPathName();
      if (real.empty())
        return std::nullopt;
      paths.insert(
          std::filesystem::path(real.str()).lexically_normal().string());
    }
  }
  return paths;
}
} // namespace

std::string recoveryInputAvailability(const RecoveryContext &context) {
  FramedHash hash;
  for (const auto &[id, file] : context.files) {
    const auto path = catalog::filePath(file);
    std::error_code error;
    hash.number("id", id);
    hash.number("exists", path && std::filesystem::exists(*path, error));
    hash.field("error", error.message());
  }
  return hash.finish();
}

void collectRecoveryScanInputs(const RecoveryContext &context,
                               const RecoveryCandidate &candidate,
                               RecoveryScan &scan) {
  RegisteredInputs registered;
  std::vector<recovery::RegisteredInput> fallback;
  for (const auto &[id, file] : context.files) {
    const auto path = catalog::filePath(file);
    recovery::RegisteredInput input{id, path ? *path : std::filesystem::path{}};
    fallback.push_back(input);
    if (path)
      registered.emplace(path->lexically_normal().string(), input);
  }
  const auto paths = inputPaths(candidate, scan, context.verbosity);
  if (!paths) {
    scan.inputs = std::move(fallback);
    return;
  }
  scan.completeInputs = scan.status == 0 && !scan.units.empty();
  std::map<FileId, recovery::RegisteredInput> closure;
  for (const auto &path : *paths) {
    const auto found = findRegisteredInput(registered, path);
    if (found == registered.end()) {
      scan.completeInputs = false;
      break;
    }
    closure.emplace(found->second.file_id, found->second);
  }
  if (!scan.completeInputs)
    scan.inputs = std::move(fallback);
  else
    for (const auto &[id, input] : closure)
      scan.inputs.push_back(input);
}
} // namespace facts::commands
