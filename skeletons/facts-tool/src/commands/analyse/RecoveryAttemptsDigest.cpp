#include "commands/analyse/RecoveryAttempts.h"
#include "commands/analyse/RecoveryAttemptsFile.h"
#include <algorithm>
#include <array>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA256.h>
#include <utility>

namespace facts::commands::recovery {
namespace {
AttemptError make_error(AttemptErrorCode code, std::string message) {
  return {code, std::move(message)};
}

std::expected<std::string, AttemptError>
path_for(const std::filesystem::path &path) {
  std::error_code ec;
  const auto result = std::filesystem::weakly_canonical(path, ec);
  return ec ? std::unexpected(make_error(
                  AttemptErrorCode::canonicalization_failed, ec.message()))
            : std::expected<std::string, AttemptError>(result.string());
}

std::string hex(std::array<std::uint8_t, 32> bytes) {
  constexpr char digits[] = "0123456789abcdef";
  std::string result;
  result.reserve(64);
  for (const auto byte : bytes) {
    result += digits[byte >> 4U];
    result += digits[byte & 0x0fU];
  }
  return result;
}
} // namespace

std::expected<std::string, AttemptError>
InputDigestCache::digest(std::span<const RegisteredInput> inputs) {
  if (inputs.empty())
    return std::unexpected(make_error(AttemptErrorCode::invalid_input,
                                      "registered input set is empty"));
  std::vector<RegisteredInput> ordered(inputs.begin(), inputs.end());
  std::ranges::sort(ordered, {}, &RegisteredInput::file_id);
  if (std::ranges::adjacent_find(ordered, {}, &RegisteredInput::file_id) !=
      ordered.end())
    return std::unexpected(make_error(AttemptErrorCode::invalid_input,
                                      "duplicate registered file id"));
  llvm::SHA256 aggregate;
  for (const auto &input : ordered) {
    auto path = path_for(input.path);
    if (!path)
      return std::unexpected(path.error());
    auto identity = inspectInput(*path);
    if (!identity)
      return std::unexpected(identity.error());
    auto cached = digests_.find(*path);
    std::string value;
    if (cached != digests_.end() && cached->second.identity == *identity) {
      value = cached->second.digest;
    } else {
      auto current = hashInput(*path);
      if (!current)
        return std::unexpected(current.error());
      ++read_count_;
      auto after = inspectInput(*path);
      if (!after)
        return std::unexpected(after.error());
      if (*after != *identity)
        return std::unexpected(
            make_error(AttemptErrorCode::hash_failed,
                       "input changed while hashing: " + *path));
      value = *current;
      digests_[*path] = {*identity, value};
    }
    aggregate.update(llvm::StringRef(std::to_string(input.file_id) + "|" +
                                     std::to_string(path->size()) + "|" +
                                     *path + "|" + value + "|"));
  }
  return hex(aggregate.final());
}

std::size_t InputDigestCache::readCount() const noexcept { return read_count_; }
} // namespace facts::commands::recovery
