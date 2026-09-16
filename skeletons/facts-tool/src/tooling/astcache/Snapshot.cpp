#include "tooling/astcache/Snapshot.h"

#include "tooling/astcache/FileIdentity.h"
#include "tooling/astcache/Metadata.h"
#include "tooling/astcache/Revisions.h"
#include "tooling/astcache/SearchDirectories.h"
#include "tooling/astcache/SnapshotGeneration.h"

#include <clang/Basic/SourceManager.h>
#include <clang/Lex/Preprocessor.h>

#include <algorithm>

namespace facts::astcache::detail {
namespace {
template <class T> void sortUnique(std::vector<T> &values) {
  std::ranges::sort(values);
  values.erase(std::ranges::unique(values).begin(), values.end());
}
} // namespace

std::expected<Snapshot, std::string>
captureSnapshot(const Entry &entry, clang::SourceManager &manager,
                clang::Preprocessor &preprocessor,
                const IncludeGraphFacts &includes) {
  if (preprocessor.getDiagnostics().hasErrorOccurred())
    return std::unexpected("cannot cache dependencies after preprocessing errors");
  Snapshot snapshot{.key = entry.key, .source = entry.source.string(),
                    .working_directory = entry.working_directory.string()};
  snapshot.inputs.push_back({snapshot.source});
  for (auto input = manager.fileinfo_begin(); input != manager.fileinfo_end();
       ++input) {
    const auto &buffer = *input->second;
    if (buffer.OrigEntry)
      snapshot.inputs.push_back(
          {resolve(buffer.OrigEntry->getName().str(), entry.working_directory).string()});
  }
  for (const auto &source : includes.visitedSources)
    snapshot.inputs.push_back({source});
  for (const auto &edge : includes.edges)
    snapshot.includes.push_back({edge.source, edge.destination});
  sortUnique(snapshot.inputs);
  sortUnique(snapshot.includes);
  const auto searchDirectories =
      headerSearchDirectories(preprocessor, entry.working_directory);
  return captureRevisions(entry.source, snapshot.inputs, searchDirectories)
      .transform([&](std::vector<Revision> revisions) {
        snapshot.revisions = std::move(revisions);
        snapshot.generation = snapshotGeneration(snapshot);
        return std::move(snapshot);
      });
}

bool currentSnapshot(const Snapshot &snapshot) {
  return !snapshot.key.empty() && !snapshot.source.empty() &&
         !snapshot.inputs.empty() &&
         snapshot.generation == snapshotGeneration(snapshot) &&
         currentRevisions(snapshot.revisions);
}

IncludeGraphFacts includesFromSnapshot(const Snapshot &snapshot) {
  IncludeGraphFacts includes;
  for (const auto &input : snapshot.inputs)
    includes.visitedSources.push_back(input.path);
  for (const auto &edge : snapshot.includes)
    includes.edges.push_back({edge.source, edge.target});
  return includes;
}
} // namespace facts::astcache::detail
