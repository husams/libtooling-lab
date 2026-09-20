#include "apis/config/Encode.h"
#include "apis/config/Logging.h"
#include <yaml-cpp/yaml.h>

namespace facts::apis {
std::string encodeSettings(const Settings &settings) {
  auto root = std::filesystem::exists(settings.serverConfig)
      ? YAML::LoadFile(settings.serverConfig.string()) : YAML::Node(YAML::NodeType::Map);
  if (!root.IsMap()) throw std::runtime_error("server configuration must be a map");
  root.remove("token");
  root.remove("daemon");
  root.remove("watch_directories");
  root["schema_version"] = 1;
  root["host"] = settings.host;
  root["port"] = settings.port;
  root["working_directory"] = settings.workingDirectory.string();
  writeLogging(root, settings.logging);
  auto watch = root["watch"];
  watch["enabled"] = settings.watchEnabled;
  watch["exclude_repositories"] = settings.excludedRepositories;
  watch["exclude_clones"] = settings.excludedClones;
  watch["exclude_directories"] = settings.excludedDirectories;
  watch["exclude_patterns"] = settings.excludePatterns;
  root["defaults"] = settings.defaults;
  root["import_arguments"] = settings.importArguments;
  root["extract_arguments"] = settings.extractArguments;
  root["debounce_ms"] = settings.debounceMs;
  root["timeout_seconds"] = settings.timeoutSeconds;
  YAML::Emitter output;
  output << root;
  if (!output.good()) throw std::runtime_error(output.GetLastError());
  return std::string(output.c_str()) + "\n";
}
}
