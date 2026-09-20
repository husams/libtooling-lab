#include "apis/config/Persistence.h"
#include "apis/config/Defaults.h"
#include "apis/config/Watch.h"
#include <yaml-cpp/yaml.h>

namespace facts::apis {
std::expected<Settings, std::string>
loadSettings(const std::filesystem::path &path) {
  try {
    Settings settings;
    settings.serverConfig = std::filesystem::weakly_canonical(path);
    settings.workingDirectory = std::filesystem::current_path();
    if (!std::filesystem::exists(path)) return settings;
    const auto root = YAML::LoadFile(path.string());
    if (!root.IsMap()) throw std::runtime_error("server configuration must be a map");
    if (root["schema_version"] && root["schema_version"].as<unsigned>() != 1)
      throw std::runtime_error("unsupported server schema_version; expected 1");
    if (root["host"]) settings.host = root["host"].as<std::string>();
    if (root["port"]) {
      const auto port = root["port"].as<int>();
      if (port < 0 || port > 65535)
        throw std::runtime_error("port must be between 0 and 65535");
      settings.port = static_cast<std::uint16_t>(port);
    }
    if (root["working_directory"])
      settings.workingDirectory = root["working_directory"].as<std::string>();
    readWatch(root["watch"], settings);
    if (root["defaults"]) settings.defaults = root["defaults"].as<std::vector<std::string>>();
    if (root["import_arguments"])
      settings.importArguments = root["import_arguments"].as<std::vector<std::string>>();
    if (root["extract_arguments"])
      settings.extractArguments = root["extract_arguments"].as<std::vector<std::string>>();
    if (root["debounce_ms"]) settings.debounceMs = root["debounce_ms"].as<unsigned>();
    if (root["timeout_seconds"])
      settings.timeoutSeconds = root["timeout_seconds"].as<unsigned>();
    return validateDefaults(settings.defaults).transform([&] { return settings; });
  } catch (const std::exception &error) {
    return std::unexpected(path.string() + ": " + error.what());
  }
}
}
