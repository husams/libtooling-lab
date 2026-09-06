#pragma once
#include "commands/analyse/RecoveryAttempts.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
using namespace facts::commands::recovery;
namespace recovery_attempts_test {
inline std::filesystem::path unique_root() {
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  for (std::size_t attempt = 0;; ++attempt) {
    const auto path = std::filesystem::temp_directory_path() /
                      ("facts-recovery-attempts-" + std::to_string(stamp) +
                       "-" + std::to_string(attempt));
    std::error_code ec;
    if (std::filesystem::create_directory(path, ec))
      return path;
    if (ec && ec != std::errc::file_exists)
      throw std::filesystem::filesystem_error("create_directory", path, ec);
    if (!ec)
      throw std::filesystem::filesystem_error(
          "create_directory", path, std::make_error_code(std::errc::io_error));
  }
}
struct Fixture {
  std::filesystem::path root = unique_root();
  std::filesystem::path source = root / "source.cpp";
  std::filesystem::path header = root / "header.hpp";
  std::filesystem::path facts = root / "facts.sqlite";
  std::filesystem::path other = root / "other";
  AttemptInput input() const {
    return {{root / "project", facts}, 7, "/usr/bin/clang++", root,
            {"-std=c++23", source.string()}, "registry-v1",
            {{7, source}, {8, header}}, {"usr:target"}};
  }
  Fixture() {
    std::filesystem::create_directories(root / "project");
    std::filesystem::create_directories(other);
    std::ofstream(source) << "int source = 1;\n";
    std::ofstream(header) << "int header = 1;\n";
    std::ofstream(facts) << "facts-v1";
  }
  ~Fixture() { std::filesystem::remove_all(root); }
};
inline void expectHit(AttemptCache &cache, const AttemptInput &input,
                      AttemptOutcome outcome) {
  auto hit = cache.lookup(input);
  assert(hit && hit->has_value() && hit->value().outcome == outcome);
}
inline bool miss(AttemptCache &cache, const AttemptInput &input) {
  auto result = cache.lookup(input);
  return result && !result->has_value();
}
} // namespace recovery_attempts_test
