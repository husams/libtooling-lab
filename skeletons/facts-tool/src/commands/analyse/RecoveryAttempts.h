#pragma once

#include "model/SymbolId.h"

#include <compare>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace facts::commands::recovery {
struct FactPairPaths {
  std::filesystem::path project;
  std::filesystem::path facts;
};
struct RegisteredInput {
  FileId file_id = 0;
  std::filesystem::path path;
};
using RequestedUsrs = std::set<std::string>;
struct AttemptInput {
  FactPairPaths pair;
  FileId translation_unit = 0;
  std::string driver_identity;
  std::filesystem::path working_directory;
  std::vector<std::string> effective_argv;
  std::string registry_fingerprint;
  std::vector<RegisteredInput> inputs;
  RequestedUsrs requested_usrs;
};
struct AttemptKey {
  std::string project;
  std::string facts;
  FileId translation_unit = 0;
  std::string driver_identity;
  std::string working_directory;
  std::vector<std::string> effective_argv;
  std::string registry_fingerprint;
  std::string input_digest;
  friend bool operator==(const AttemptKey &, const AttemptKey &) = default;
  friend auto operator<=>(const AttemptKey &, const AttemptKey &) = default;
};
enum class AttemptOutcome { no_match, succeeded, failed };
struct AttemptDiagnostic {
  std::string code;
  std::string message;
};
struct AttemptRecord {
  AttemptOutcome outcome = AttemptOutcome::failed;
  AttemptDiagnostic diagnostic;
  RequestedUsrs requested_usrs;
};
enum class AttemptErrorCode {
  invalid_input,
  canonicalization_failed,
  missing_input,
  unreadable_input,
  hash_failed,
};
struct AttemptError {
  AttemptErrorCode code;
  std::string message;
};
class InputDigestCache {
public:
  std::expected<std::string, AttemptError> digest(
      std::span<const RegisteredInput> inputs);
private:
  std::map<std::string, std::string> digests_;
};
class KeyBuilder {
public:
  std::expected<AttemptKey, AttemptError> build(const AttemptInput &input) const;
private:
  mutable InputDigestCache digest_cache_;
};
class AttemptCache {
public:
  std::expected<AttemptKey, AttemptError> build(const AttemptInput &input);
  std::optional<AttemptRecord> find(const AttemptKey &key, const RequestedUsrs &requested) const;
  std::expected<void, AttemptError> record(
      const AttemptKey &key, RequestedUsrs requested, AttemptOutcome outcome,
      AttemptDiagnostic diagnostic);
  std::expected<std::optional<AttemptRecord>, AttemptError> lookup(
      const AttemptInput &input);
  std::expected<void, AttemptError>
  record(const AttemptInput &input, AttemptOutcome outcome,
         AttemptDiagnostic diagnostic);
  std::size_t size() const noexcept;
private:
  KeyBuilder builder_;
  std::map<AttemptKey, std::map<RequestedUsrs, AttemptRecord>> attempts_;
};
} // namespace facts::commands::recovery
