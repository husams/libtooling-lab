#include "commands/analyse/RecoveryAttempts.h"

#include <algorithm>
#include <utility>

namespace facts::commands::recovery {
std::expected<AttemptKey, AttemptError>
AttemptCache::build(const AttemptInput &input) {
  return builder_.build(input);
}

std::optional<AttemptRecord>
AttemptCache::find(const AttemptKey &key,
                   const RequestedUsrs &requested) const {
  const auto found = attempts_.find(key);
  if (found == attempts_.end())
    return std::nullopt;
  const auto result = found->second.find(requested);
  if (result != found->second.end())
    return result->second;
  for (const auto &[wanted, record] : found->second) {
    if (record.outcome != AttemptOutcome::no_match)
      continue;
    const bool covered = std::ranges::all_of(
        requested, [&](const auto &usr) { return wanted.contains(usr); });
    if (covered)
      return record;
  }
  return std::nullopt;
}

std::expected<void, AttemptError>
AttemptCache::record(const AttemptKey &key, RequestedUsrs requested,
                     AttemptOutcome outcome, AttemptDiagnostic diagnostic) {
  attempts_[key][requested] =
      AttemptRecord{outcome, std::move(diagnostic), requested};
  return {};
}

std::expected<std::optional<AttemptRecord>, AttemptError>
AttemptCache::lookup(const AttemptInput &input) {
  auto key = build(input);
  if (!key)
    return std::unexpected(key.error());
  return find(*key, input.requested_usrs);
}

std::expected<void, AttemptError>
AttemptCache::record(const AttemptInput &input, AttemptOutcome outcome,
                     AttemptDiagnostic diagnostic) {
  auto key = build(input);
  if (!key)
    return std::unexpected(key.error());
  return record(*key, input.requested_usrs, outcome, std::move(diagnostic));
}

std::size_t AttemptCache::size() const noexcept { return attempts_.size(); }
} // namespace facts::commands::recovery
