#include "config/Configuration.h"
#include "config/ConfigurationYamlEvents.h"

#include <yaml-cpp/yaml.h>
#include <yaml-cpp/parser.h>
#include <fstream>
#include <set>
#include <regex>

namespace facts::config {
namespace {
std::expected<void, std::string> scalar(const YAML::Node &node,
                                        std::string_view key) {
  if (!node || node.IsNull() || !node.IsScalar())
    return std::unexpected(std::string(key) + " must be a nonempty string");
  if ((key != "extra_args" && node.Scalar().empty()) ||
      node.Scalar().find('\0') != std::string::npos ||
      node.Scalar().find('\n') != std::string::npos)
    return std::unexpected(std::string(key) + " must be a nonempty string");
  static const std::regex nonString(
      R"(^([+-]?([0-9][0-9_]*(\.[0-9_]*)?([eE][+-]?[0-9]+)?|\.[0-9]+([eE][+-]?[0-9]+)?|0[xX][0-9a-fA-F_]+|0[oO][0-7_]+|\.inf)|\.nan|true|false|yes|no|on|off)$)",
      std::regex::icase);
  if (node.Tag() == "?" && std::regex_match(node.Scalar(), nonString))
    return std::unexpected(std::string(key) + " must be a string (quote numeric/boolean tokens)");
  return {};
}
}

std::expected<Resolved, std::string>
readYaml(const std::filesystem::path &path, Resolved result) {
  try {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error))
      return std::unexpected("cannot read configuration: not a regular file: " + path.string());
    std::ifstream input(path);
    if (!input)
      return std::unexpected("cannot read configuration: " + path.string());
    YAML::Parser parser(input);
    SafeYamlEvents events;
    while (parser.HandleNextDocument(events)) {}
    if (events.invalid)
      return std::unexpected("configuration must not use YAML tags, anchors, or aliases");
    input.clear();
    input.seekg(0);
    const auto documents = YAML::LoadAll(input);
    if (documents.empty())
      return result;
    if (documents.size() != 1)
      return std::unexpected("configuration must contain one YAML document");
    const auto root = documents.front();
    if (!root || root.IsNull())
      return result;
    if (!root.IsMap())
      return std::unexpected("configuration must be a mapping");
    std::set<std::string> keys;
    for (const auto &entry : root) {
      if (!entry.first.IsScalar() || !keys.insert(entry.first.Scalar()).second)
        return std::unexpected("duplicate or invalid configuration key");
      const auto key = entry.first.Scalar();
      if (key != "conf_root" && key != "conf_template" && key != "extra_args")
        return std::unexpected("unknown configuration key: " + key);
      if (key != "extra_args") {
        if (!result.generated) continue;
        if (auto valid = scalar(entry.second, key); !valid)
          return std::unexpected(valid.error());
        if (key == "conf_root") {
          result.storageRoot = entry.second.Scalar();
          result.storageRootSource = path.string() + ": conf_root";
        }
        if (key == "conf_template") {
          result.templateText = entry.second.Scalar();
          result.templateSource = path.string() + ": conf_template";
        }
      } else {
        if (!entry.second || entry.second.IsNull() || !entry.second.IsSequence())
          return std::unexpected("extra_args must be a sequence of strings");
        result.extraArguments.clear();
        for (const auto &value : entry.second) {
          if (auto valid = scalar(value, "extra_args"); !valid)
            return std::unexpected(valid.error());
          result.extraArguments.push_back(value.Scalar());
        }
        result.extraArgumentsSource = path.string() + ": extra_args";
      }
    }
    return result;
  } catch (const YAML::Exception &error) {
    return std::unexpected(error.what());
  }
}

} // namespace facts::config
