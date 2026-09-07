#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "commands/analyse/RecoveryRegistryHash.h"
#include "commands/analyse/RecoveryScan.h"
#include "storage/catalog/File.h"
#include <clang/Basic/FileEntry.h>

namespace facts::commands {
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
  std::map<std::string, recovery::RegisteredInput> registered;
  std::vector<recovery::RegisteredInput> fallback;
  for (const auto &[id, file] : context.files) {
    const auto path = catalog::filePath(file);
    recovery::RegisteredInput input{id, path ? *path : std::filesystem::path{}};
    fallback.push_back(input);
    if (path)
      registered.emplace(path->lexically_normal().string(), input);
  }
  std::set<std::string> paths{candidate.source.lexically_normal().string()};
  for (const auto &unit : scan.units) {
    const auto &manager = unit->getSourceManager();
    for (auto it = manager.fileinfo_begin(); it != manager.fileinfo_end();
         ++it) {
      const auto &file = it->first.getFileEntry();
      const auto real = file.tryGetRealPathName();
      if (real.empty()) {
        scan.inputs = std::move(fallback);
        return;
      }
      paths.insert(
          std::filesystem::path(real.str()).lexically_normal().string());
    }
  }
  scan.completeInputs = scan.status == 0 && !scan.units.empty();
  std::map<FileId, recovery::RegisteredInput> closure;
  for (const auto &path : paths) {
    const auto found = registered.find(path);
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
