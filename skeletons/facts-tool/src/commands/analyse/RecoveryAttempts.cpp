#include "commands/analyse/RecoveryAttempts.h"
#include <algorithm>
#include <utility>
namespace facts::commands::recovery {
namespace {
AttemptError error(AttemptErrorCode code, std::string message) {
  return {code, std::move(message)};
}
std::expected<std::string, AttemptError> canonical(
    const std::filesystem::path &path, std::string_view label) {
  std::error_code ec;
  const auto value = std::filesystem::weakly_canonical(path, ec);
  return ec ? std::unexpected(error(AttemptErrorCode::canonicalization_failed,
                                    std::string(label) + ": " + ec.message()))
            : std::expected<std::string, AttemptError>(value.string());
}
} // namespace
std::expected<AttemptKey, AttemptError> KeyBuilder::build(
    const AttemptInput &input) const {
  const auto tu = std::ranges::find_if(
      input.inputs, [&](const auto &candidate) {
        return candidate.file_id == input.translation_unit;
      });
  if (tu == input.inputs.end())
    return std::unexpected(error(AttemptErrorCode::invalid_input,
                                  "registered input set omits translation unit"));
  auto project = canonical(input.pair.project, "project path");
  if (!project)
    return std::unexpected(project.error());
  auto facts = canonical(input.pair.facts, "facts path");
  if (!facts)
    return std::unexpected(facts.error());
  auto working = canonical(input.working_directory, "working directory");
  if (!working)
    return std::unexpected(working.error());
  auto content = digest_cache_.digest(input.inputs);
  if (!content)
    return std::unexpected(content.error());
  return AttemptKey{*project, *facts, input.translation_unit, input.driver_identity,
                    *working, input.effective_argv, input.registry_fingerprint,
                    *content};
}
std::expected<AttemptKey, AttemptError> AttemptCache::build(
    const AttemptInput &input) {
  return builder_.build(input);
}
std::optional<AttemptRecord> AttemptCache::find(
    const AttemptKey &key, const RequestedUsrs &requested) const {
  const auto found = attempts_.find(key);
  if (found == attempts_.end())
    return std::nullopt;
  const auto result = found->second.find(requested);
  return result == found->second.end() ? std::nullopt
                                       : std::optional<AttemptRecord>{result->second};
}
std::expected<void, AttemptError> AttemptCache::record(
    const AttemptKey &key, RequestedUsrs requested, AttemptOutcome outcome,
    AttemptDiagnostic diagnostic) {
  attempts_[key][requested] =
      AttemptRecord{outcome, std::move(diagnostic), requested};
  return {};
}
std::expected<std::optional<AttemptRecord>, AttemptError> AttemptCache::lookup(
    const AttemptInput &input) {
  auto key = build(input);
  if (!key)
    return std::unexpected(key.error());
  return find(*key, input.requested_usrs);
}
std::expected<void, AttemptError> AttemptCache::record(
    const AttemptInput &input, AttemptOutcome outcome,
    AttemptDiagnostic diagnostic) {
  auto key = build(input);
  if (!key)
    return std::unexpected(key.error());
  return record(*key, input.requested_usrs, outcome, std::move(diagnostic));
}
std::size_t AttemptCache::size() const noexcept { return attempts_.size(); }
} // namespace facts::commands::recovery
