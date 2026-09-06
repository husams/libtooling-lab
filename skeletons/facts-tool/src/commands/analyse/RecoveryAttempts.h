#pragma once

#include "commands/analyse/RecoveryAttemptsTypes.h"

#include <cstddef>
#include <expected>
#include <map>
#include <optional>
#include <span>

namespace facts::commands::recovery {
class InputDigestCache {
public:
  std::expected<std::string, AttemptError>
  digest(std::span<const RegisteredInput> inputs);
  std::size_t readCount() const noexcept;

private:
  struct CachedDigest {
    InputFileIdentity identity;
    std::string digest;
  };

  std::map<std::string, CachedDigest> digests_;
  std::size_t read_count_ = 0;
};

class KeyBuilder {
public:
  std::expected<AttemptKey, AttemptError>
  build(const AttemptInput &input) const;

private:
  mutable InputDigestCache digest_cache_;
};

class AttemptCache {
public:
  std::expected<AttemptKey, AttemptError> build(const AttemptInput &input);
  std::optional<AttemptRecord> find(const AttemptKey &key,
                                    const RequestedUsrs &requested) const;
  std::expected<void, AttemptError> record(const AttemptKey &key,
                                           RequestedUsrs requested,
                                           AttemptOutcome outcome,
                                           AttemptDiagnostic diagnostic);
  std::expected<std::optional<AttemptRecord>, AttemptError>
  lookup(const AttemptInput &input);
  std::expected<void, AttemptError> record(const AttemptInput &input,
                                           AttemptOutcome outcome,
                                           AttemptDiagnostic diagnostic);
  std::size_t size() const noexcept;

private:
  KeyBuilder builder_;
  std::map<AttemptKey, std::map<RequestedUsrs, AttemptRecord>> attempts_;
};
} // namespace facts::commands::recovery
