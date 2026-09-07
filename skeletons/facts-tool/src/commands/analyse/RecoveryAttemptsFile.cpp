#include "commands/analyse/RecoveryAttemptsFile.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/SHA256.h>
#include <sys/stat.h>
#include <utility>

namespace facts::commands::recovery {
namespace {
AttemptError failure(AttemptErrorCode code, const std::string &path) {
  return {code, "cannot read input: " + path};
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

std::int64_t mtimeNanoseconds(const struct stat &value) {
#if defined(__APPLE__)
  return value.st_mtimespec.tv_nsec;
#else
  return value.st_mtim.tv_nsec;
#endif
}

std::int64_t ctimeNanoseconds(const struct stat &value) {
#if defined(__APPLE__)
  return value.st_ctimespec.tv_nsec;
#else
  return value.st_ctim.tv_nsec;
#endif
}
} // namespace

std::expected<InputFileIdentity, AttemptError>
inspectInput(const std::string &path) {
  struct stat value = {};
  if (::stat(path.c_str(), &value) != 0)
    return std::unexpected(failure(errno == ENOENT
                                       ? AttemptErrorCode::missing_input
                                       : AttemptErrorCode::unreadable_input,
                                   path));
  if (!S_ISREG(value.st_mode))
    return std::unexpected(failure(AttemptErrorCode::unreadable_input, path));
#if defined(__APPLE__)
  const auto mtime_seconds = value.st_mtimespec.tv_sec;
  const auto ctime_seconds = value.st_ctimespec.tv_sec;
#else
  const auto mtime_seconds = value.st_mtim.tv_sec;
  const auto ctime_seconds = value.st_ctim.tv_sec;
#endif
  return InputFileIdentity{static_cast<std::uintmax_t>(value.st_size),
                           static_cast<std::uintmax_t>(value.st_ino),
                           mtime_seconds,
                           mtimeNanoseconds(value),
                           ctime_seconds,
                           ctimeNanoseconds(value)};
}

std::expected<std::string, AttemptError> hashInput(const std::string &path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream)
    return std::unexpected(failure(AttemptErrorCode::unreadable_input, path));
  llvm::SHA256 sha;
  std::array<char, 16384> buffer = {};
  while (stream.read(buffer.data(), buffer.size()) || stream.gcount() != 0)
    sha.update(llvm::StringRef(buffer.data(), stream.gcount()));
  if (!stream.eof())
    return std::unexpected(failure(AttemptErrorCode::hash_failed, path));
  return hex(sha.final());
}
} // namespace facts::commands::recovery
