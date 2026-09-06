#include "commands/analyse/CallGraphRecoveryInternal.h"

#include "storage/catalog/File.h"

#include <algorithm>

namespace facts::commands {

std::vector<recovery::RegisteredInput> recoveryRegisteredInputs(
    const RecoveryContext &context, const RecoveryCandidate &candidate) {
  std::vector<recovery::RegisteredInput> inputs;
  // Coverage is unavailable, so include every registered catalog file.
  for (const auto &[id, file] : context.files) {
    const auto path = catalog::filePath(file);
    // An unresolved path must fail hashing, not disappear from the identity.
    inputs.push_back({id, path ? *path : std::filesystem::path{}});
  }
  if (!std::ranges::any_of(inputs, [&](const auto &input) {
        return input.file_id == candidate.entry.tuFileId;
      }))
    inputs.push_back({candidate.entry.tuFileId, candidate.source});
  return inputs;
}

} // namespace facts::commands
