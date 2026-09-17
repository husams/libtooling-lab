#include "tooling/astcache/Metadata.h"

#include "storage/astcache/Database.h"
#include "tooling/astcache/FileIdentity.h"
#include "tooling/astcache/Snapshot.h"

#include <utility>

namespace facts::astcache::detail {

std::expected<std::optional<Snapshot>, std::string>
readCurrentSnapshot(const Entry &entry) {
  return storage::astcache::readSnapshot(entry.database, entry.key)
      .transform([](std::optional<Snapshot> snapshot) {
        if (snapshot && !currentSnapshot(*snapshot))
          snapshot.reset();
        return snapshot;
      });
}

std::optional<Snapshot> readValidEntry(const Entry &entry) {
  auto snapshot = readCurrentSnapshot(entry);
  if (!snapshot || !*snapshot)
    return std::nullopt;
  const auto artifact = storage::astcache::readArtifact(entry.database, entry.key);
  if (!artifact || !*artifact || (*artifact)->path != entry.ast.string() ||
      (*artifact)->generation != (*snapshot)->generation)
    return std::nullopt;
  // Git commits decide freshness. Hash only the serialized artifact to reject
  // corruption; worktree file contents are never read during validation.
  const auto hash = readDigest(entry.ast);
  if (!hash || *hash != (*artifact)->digest)
    return std::nullopt;
  return std::move(*snapshot);
}

std::expected<void, std::string> writeMetadata(const Entry &entry,
                                             const Snapshot &snapshot) {
  return readDigest(entry.ast).and_then([&](const std::string &hash) {
    const Artifact artifact{entry.key, entry.ast.string(), hash,
                            snapshot.generation};
    return storage::astcache::writeArtifact(entry.database, snapshot, artifact);
  });
}

} // namespace facts::astcache::detail
