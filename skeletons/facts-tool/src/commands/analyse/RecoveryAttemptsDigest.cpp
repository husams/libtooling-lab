#include "commands/analyse/RecoveryAttempts.h"
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA256.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <utility>
namespace facts::commands::recovery {
namespace {
AttemptError make_error(AttemptErrorCode code, std::string message) {
  return {code, std::move(message)};
}
std::expected<std::string, AttemptError> path_for(
    const std::filesystem::path &path) {
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
std::expected<std::string, AttemptError> file_digest(const std::string &path) {
  std::error_code ec;
  const bool present = std::filesystem::exists(path, ec);
  if (ec)
    return std::unexpected(make_error(AttemptErrorCode::unreadable_input,
                                      "cannot inspect input: " + path));
  if (!present)
    return std::unexpected(make_error(AttemptErrorCode::missing_input,
                                      "missing input: " + path));
  if (!std::filesystem::is_regular_file(path, ec) || ec)
    return std::unexpected(make_error(AttemptErrorCode::unreadable_input,
                                      "cannot read input: " + path));
  std::ifstream stream(path, std::ios::binary);
  if (!stream)
    return std::unexpected(make_error(AttemptErrorCode::unreadable_input,
                                      "cannot read input: " + path));
  llvm::SHA256 sha;
  std::array<char, 16384> buffer{};
  while (stream.read(buffer.data(), buffer.size()) || stream.gcount() != 0)
    sha.update(llvm::StringRef(buffer.data(), stream.gcount()));
  return stream.eof() ? std::expected<std::string, AttemptError>(hex(sha.final()))
                      : std::unexpected(make_error(
                            AttemptErrorCode::hash_failed,
                            "cannot hash input: " + path));
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
    return std::unexpected(
        make_error(AttemptErrorCode::invalid_input, "duplicate registered file id"));
  llvm::SHA256 aggregate;
  for (const auto &input : ordered) {
    auto path = path_for(input.path);
    if (!path)
      return std::unexpected(path.error());
    auto current = file_digest(*path);
    if (!current)
      return std::unexpected(current.error());
    const auto cached = digests_.find(*path);
    const auto &value = cached != digests_.end() && cached->second == *current
                            ? cached->second
                            : (digests_[*path] = *current);
    aggregate.update(llvm::StringRef(std::to_string(input.file_id) + "|" +
                                     std::to_string(path->size()) + "|" +
                                     *path + "|" + value + "|"));
  }
  return hex(aggregate.final());
}
} // namespace facts::commands::recovery
