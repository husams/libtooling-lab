#pragma once

#include "model/SymbolId.h"

#include <compare>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
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

struct InputFileIdentity {
  std::uintmax_t size = 0;
  std::uintmax_t inode = 0;
  std::int64_t mtime_seconds = 0;
  std::int64_t mtime_nanoseconds = 0;
  std::int64_t ctime_seconds = 0;
  std::int64_t ctime_nanoseconds = 0;
  friend bool operator==(const InputFileIdentity &,
                         const InputFileIdentity &) = default;
};
} // namespace facts::commands::recovery
