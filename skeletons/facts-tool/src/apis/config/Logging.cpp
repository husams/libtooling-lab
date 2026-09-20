#include "apis/config/Logging.h"
#include <set>
#include <stdexcept>

namespace facts::apis {
namespace {
std::string stringValue(const YAML::Node &node, const char *name) {
  if (!node.IsScalar() || (node.Tag() != "?" && node.Tag() != "!" &&
                          node.Tag() != "tag:yaml.org,2002:str"))
    throw std::runtime_error(std::string(name) + " must be a string");
  const auto value = node.as<std::string>();
  if (value.empty() || value.find('\0') != std::string::npos)
    throw std::runtime_error(std::string(name) +
                             " must be nonempty and contain no NUL");
  return value;
}
unsigned verbosityValue(const YAML::Node &node) {
  if (!node.IsScalar() || (node.Tag() != "?" &&
                          node.Tag() != "tag:yaml.org,2002:int"))
    throw std::runtime_error("logging.verbosity must be an integer between 0 and 3");
  try {
    const auto value = node.as<int>();
    if (value >= 0 && value <= 3) return static_cast<unsigned>(value);
  } catch (const YAML::Exception &) {}
  throw std::runtime_error("logging.verbosity must be an integer between 0 and 3");
}
std::string pathValue(const YAML::Node &node) {
  const auto value = stringValue(node, "logging.file");
  bool boolean;
  double number;
  if (node.Tag() == "?" && (YAML::convert<bool>::decode(node, boolean) ||
                            YAML::convert<double>::decode(node, number)))
    throw std::runtime_error("logging.file must be a string");
  return value;
}
}
void readLogging(const YAML::Node &node, logging::Options &options) {
  if (!node) return;
  if (!node.IsMap()) throw std::runtime_error("logging must be a map");
  std::set<std::string> seen;
  for (const auto &entry : node) {
    const auto key = stringValue(entry.first, "logging key");
    if (key != "file" && key != "level" && key != "verbosity")
      throw std::runtime_error("unknown logging option: " + key);
    if (!seen.insert(key).second)
      throw std::runtime_error("duplicate logging option: " + key);
  }
  if (node["level"] && node["verbosity"])
    throw std::runtime_error("logging.level and logging.verbosity are mutually exclusive");
  if (node["file"]) options.file = pathValue(node["file"]);
  if (node["level"]) {
    const auto level = logging::parseLevel(stringValue(node["level"], "logging.level"));
    if (!level) throw std::runtime_error("logging.level: " + level.error());
    options.level = *level;
  }
  if (node["verbosity"])
    options.level = logging::verbosityLevel(verbosityValue(node["verbosity"]));
}
void writeLogging(YAML::Node &root, const logging::Options &options) {
  auto node = root["logging"];
  node.remove("verbosity");
  node["level"] = std::string(logging::levelName(options.level));
  if (options.file.empty()) node.remove("file");
  else node["file"] = options.file.string();
}
}
