#include "config/Configuration.h"
#include "config/ConfigurationPlaceholders.h"
#include "config/ConfigurationShape.h"
#include "config/ConfigurationYamlEvents.h"

#include <yaml-cpp/yaml.h>
#include <yaml-cpp/parser.h>
#include <fstream>
#include <set>
#include <regex>

namespace facts::config {
namespace {
// Shape and dummy-substitution checks so an invalid template (a .. component,
// unknown placeholder, unmatched braces, unset ${ENV}) is a configuration
// error for the tier that declared it, even if a higher tier's value wins
// the merge (B-030 C-3117): none of these depend on the real project context.
std::expected<void, std::string> templateSyntax(std::string_view key, const std::string &text) {
  if (detail::invalidPathShape(text))
    return std::unexpected(std::string(key) + " template must not be empty, end with a separator, "
                           "or contain a .. component");
  return detail::expandPlaceholders(text, {})
      .transform([](auto &&) {})
      .transform_error([&](auto &&reason) { return std::string(key) + " " + reason; });
}

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

std::expected<Tier, std::string> readTier(const std::filesystem::path &path,
                                          bool applyPathSettings) {
  Tier tier{.path = path};
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
      return tier;
    if (documents.size() != 1)
      return std::unexpected("configuration must contain one YAML document");
    const auto root = documents.front();
    if (!root || root.IsNull())
      return tier;
    if (!root.IsMap())
      return std::unexpected("configuration must be a mapping");
    std::set<std::string> keys;
    for (const auto &entry : root) {
      if (!entry.first.IsScalar() || !keys.insert(entry.first.Scalar()).second)
        return std::unexpected("duplicate or invalid configuration key");
      const auto key = entry.first.Scalar();
      if (key != "conf_root" && key != "conf_template" && key != "facts_template" &&
          key != "extra_args")
        return std::unexpected("unknown configuration key: " + key);
      // facts_template governs the separate facts database and stays in
      // effect regardless of a direct --conf/FACTS_TOOL_CONF override,
      // which only bypasses the generated project *configuration* database
      // (conf_root/conf_template); like extra_args it is always consumed.
      if (key == "conf_root" || key == "conf_template") {
        if (!applyPathSettings) continue;
        if (auto valid = scalar(entry.second, key); !valid)
          return std::unexpected(valid.error());
        if (key == "conf_root") tier.confRoot = entry.second.Scalar();
        if (key == "conf_template") {
          if (auto valid = templateSyntax(key, entry.second.Scalar()); !valid)
            return std::unexpected(valid.error());
          tier.confTemplate = entry.second.Scalar();
        }
      } else if (key == "facts_template") {
        if (auto valid = scalar(entry.second, key); !valid)
          return std::unexpected(valid.error());
        if (auto valid = templateSyntax(key, entry.second.Scalar()); !valid)
          return std::unexpected(valid.error());
        tier.factsTemplate = entry.second.Scalar();
      } else {
        if (!entry.second || entry.second.IsNull() || !entry.second.IsSequence())
          return std::unexpected("extra_args must be a sequence of strings");
        std::vector<std::string> values;
        for (const auto &value : entry.second) {
          if (auto valid = scalar(value, "extra_args"); !valid)
            return std::unexpected(valid.error());
          values.push_back(value.Scalar());
        }
        tier.extraArgs = std::move(values);
      }
    }
    return tier;
  } catch (const YAML::Exception &error) {
    return std::unexpected(error.what());
  }
}

} // namespace facts::config
