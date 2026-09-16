#include "tooling/astcache/SnapshotGeneration.h"

#include "tooling/astcache/FileIdentity.h"

#include <string_view>

namespace facts::astcache::detail {
namespace {
void append(std::string &bytes, std::string_view value) {
  bytes += std::to_string(value.size());
  bytes += ':';
  bytes += value;
}
} // namespace

std::string snapshotGeneration(const Snapshot &snapshot) {
  std::string bytes;
  append(bytes, snapshot.key);
  append(bytes, snapshot.source);
  append(bytes, snapshot.working_directory);
  append(bytes, "inputs");
  append(bytes, std::to_string(snapshot.inputs.size()));
  for (const auto &input : snapshot.inputs)
    append(bytes, input.path);
  append(bytes, "includes");
  append(bytes, std::to_string(snapshot.includes.size()));
  for (const auto &include : snapshot.includes) {
    append(bytes, include.source);
    append(bytes, include.target);
  }
  append(bytes, "revisions");
  append(bytes, std::to_string(snapshot.revisions.size()));
  for (const auto &revision : snapshot.revisions) {
    append(bytes, revision.path);
    append(bytes, revision.commit);
  }
  return digest(bytes);
}
} // namespace facts::astcache::detail
