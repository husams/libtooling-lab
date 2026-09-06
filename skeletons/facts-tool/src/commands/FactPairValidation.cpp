#include "commands/FactPairValidation.h"

#include "commands/FactPairValidationInternal.h"
#include "storage/FactStore.h"

#include <filesystem>

namespace facts::commands {
namespace {

std::expected<FactPairProvenanceSnapshot, std::string>
snapshotVector(const detail::ProvenanceSnapshot &snapshot) {
  FactPairProvenanceSnapshot result;
  result.reserve(snapshot.size());
  for (const auto &[file, value] : snapshot) {
    result.push_back({file, value.path, value.universe});
  }
  return result;
}

std::expected<void, std::string> validate(const std::string &facts,
                                          const std::string &project) {
  auto snapshot = detail::loadProjectProvenance(project);
  if (!snapshot) {
    return std::unexpected(snapshot.error());
  }
  std::error_code error;
  if (!std::filesystem::exists(facts, error)) {
    if (error) {
      return std::unexpected("cannot inspect facts database: " +
                             error.message());
    }
    return {};
  }
  auto database = storage::Database::open(facts, storage::Database::readOnly);
  if (!database) {
    return std::unexpected("cannot open facts database: " +
                           database.error().message());
  }
  if (auto version = detail::factsSchemaVersion(*database); !version) {
    return std::unexpected(version.error());
  }
  return detail::validateFactsProvenance(*database, *snapshot, true);
}

} // namespace

std::expected<FactPairProvenanceSnapshot, std::string>
prepareFactPairForWrite(const std::string &facts, const std::string &project) {
  auto checked = validate(facts, project);
  if (!checked) {
    return std::unexpected(checked.error());
  }
  auto snapshot = detail::loadProjectProvenance(project);
  if (!snapshot) {
    return std::unexpected(snapshot.error());
  }
  return snapshotVector(*snapshot);
}

std::expected<void, std::string>
registerFactPairProvenance(FactStore &store,
                           const FactPairProvenanceSnapshot &snapshot,
                           std::span<const FileId> selected) {
  auto registered = store.registerFactProvenance(snapshot, selected);
  if (!registered) {
    return std::unexpected("cannot register facts provenance: " +
                           registered.error().message());
  }
  return {};
}

std::expected<void, std::string>
validateFactPairForRead(const std::string &facts, const std::string &project) {
  auto snapshot = detail::loadProjectProvenance(project);
  if (!snapshot) {
    return std::unexpected(snapshot.error());
  }
  auto database = storage::Database::open(facts, storage::Database::readOnly);
  if (!database) {
    return std::unexpected("cannot open facts database: " +
                           database.error().message());
  }
  if (auto version = detail::factsSchemaVersion(*database); !version) {
    return std::unexpected(version.error());
  }
  return detail::validateFactsProvenance(*database, *snapshot, false);
}

std::expected<void, std::string>
validateFactPairForWrite(const std::string &facts, const std::string &project) {
  return prepareFactPairForWrite(facts, project).transform([](auto) {});
}

} // namespace facts::commands
