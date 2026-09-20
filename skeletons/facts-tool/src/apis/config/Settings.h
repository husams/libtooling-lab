#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace facts::apis {
struct Settings {
  std::filesystem::path serverConfig;
  std::filesystem::path executable;
  std::filesystem::path workingDirectory;
  std::string host = "127.0.0.1";
  std::uint16_t port = 0;
  bool explicitPort = false;
  bool daemon = false;
  std::string token;
  std::vector<std::string> defaults;
#ifdef __linux__
  bool watchEnabled = true;
#else
  bool watchEnabled = false;
#endif
  std::vector<std::string> excludedRepositories;
  std::vector<std::string> excludedClones;
  std::vector<std::string> excludedDirectories;
  std::vector<std::string> excludePatterns;
  std::vector<std::string> importArguments;
  std::vector<std::string> extractArguments;
  unsigned debounceMs = 500;
  unsigned timeoutSeconds = 3600;
};
}
