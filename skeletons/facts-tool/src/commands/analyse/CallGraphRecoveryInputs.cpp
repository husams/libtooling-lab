#include "commands/IncludedFiles.h"
#include "commands/analyse/CallGraphRecoveryInternal.h"
#include "commands/analyse/RecoveryCompilation.h"
#include "storage/catalog/File.h"

namespace facts::commands {
std::vector<recovery::RegisteredInput>
recoveryRegisteredInputs(RecoveryContext &context,
                         const RecoveryCandidate &candidate) {
  const auto id = candidate.entry.tuFileId;
  context.inputClosures.erase(id);
  context.closureDigests.erase(id);
  std::map<std::string, recovery::RegisteredInput> registered;
  std::vector<recovery::RegisteredInput> fallback;
  for (const auto &[fileId, file] : context.files) {
    const auto path = catalog::filePath(file);
    recovery::RegisteredInput input{fileId,
                                    path ? *path : std::filesystem::path{}};
    fallback.push_back(input);
    if (path)
      registered.emplace(path->lexically_normal().string(), input);
  }
  RecoveryCompilation database(candidate);
  const std::vector<std::string> sources{candidate.source.string()};
  auto visited = discoverIncludedFiles(database, sources);
  if (!visited)
    return fallback; // Preprocessing did not establish complete input coverage.
  visited->push_back(candidate.source.string());
  std::map<FileId, recovery::RegisteredInput> closure;
  for (const auto &path : *visited) {
    const auto file = registered.find(
        std::filesystem::path(path).lexically_normal().string());
    if (file == registered.end())
      return fallback; // An input has no registered identity.
    closure.emplace(file->second.file_id, file->second);
  }
  std::vector<recovery::RegisteredInput> inputs;
  for (const auto &[fileId, input] : closure)
    inputs.push_back(input);
  auto digest = context.digests.digest(inputs);
  if (digest) {
    context.inputClosures[id] = inputs;
    context.closureDigests[id] = *digest;
  }
  return inputs;
}
} // namespace facts::commands
