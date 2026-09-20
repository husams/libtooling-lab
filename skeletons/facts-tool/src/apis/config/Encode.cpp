#include "apis/config/Encode.h"
#include <yaml-cpp/yaml.h>

namespace facts::apis {
std::string encodeSettings(const Settings &settings) {
  auto root = std::filesystem::exists(settings.serverConfig)
      ? YAML::LoadFile(settings.serverConfig.string()) : YAML::Node(YAML::NodeType::Map);
  if (!root.IsMap()) throw std::runtime_error("server configuration must be a map");
  root.remove("token");
  root.remove("daemon");
  root["schema_version"] = 1;
  root["host"] = settings.host;
  root["port"] = settings.port;
  root["working_directory"] = settings.workingDirectory.string();
  root["watch_directories"] = YAML::Node(YAML::NodeType::Sequence);
  for (const auto &directory : settings.directories)
    root["watch_directories"].push_back(directory.string());
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
