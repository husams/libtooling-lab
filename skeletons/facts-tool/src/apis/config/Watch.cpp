#include "apis/config/Watch.h"
#include <stdexcept>

namespace facts::apis {
namespace {
void readList(const YAML::Node &watch, const char *key,
              std::vector<std::string> &values) {
  const auto node = watch[key];
  if (!node) return;
  if (!node.IsSequence())
    throw std::runtime_error(std::string("watch.") + key + " must be a sequence");
  values = node.as<std::vector<std::string>>();
  for (const auto &value : values)
    if (value.empty() || value.find('\0') != std::string::npos)
      throw std::runtime_error(std::string("watch.") + key +
                               " must contain nonempty strings without NUL");
}
}
void readWatch(const YAML::Node &watch, Settings &settings) {
  if (!watch) return;
  if (!watch.IsMap()) throw std::runtime_error("watch must be a map");
  if (watch["enabled"]) settings.watchEnabled = watch["enabled"].as<bool>();
  readList(watch, "exclude_repositories", settings.excludedRepositories);
  readList(watch, "exclude_clones", settings.excludedClones);
  readList(watch, "exclude_directories", settings.excludedDirectories);
  readList(watch, "exclude_patterns", settings.excludePatterns);
}
}
