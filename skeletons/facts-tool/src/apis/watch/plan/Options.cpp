#include "apis/watch/plan/Details.h"
#include "apis/watch/Paths.h"
#include <set>

namespace facts::apis::watch::plan {
std::expected<Options, std::string>
options(const Settings &settings, bool importing) {
  const auto &input = importing ? settings.importArguments : settings.extractArguments;
  const std::set<std::string> values{"--config", "--extra-arg", "-v", "--verbose"};
  Options result;
  for (std::size_t index = 0; index < input.size(); ++index) {
    const auto &argument = input[index];
    const auto separator = argument.find('=');
    const auto key = argument.substr(0, separator);
    if (key == "--no-ast-cache" || (!importing && key == "--force")) continue;
    const bool database = importing && (key == "-p" || key == "--compilation-database");
    const bool output = importing ? key == "-f" || key == "--facts"
                                  : key == "-o" || key == "--output";
    if (!database && !output && !values.contains(key))
      return std::unexpected("watch arguments require supported options; source "
                             "selection and clone identity come from the database: " + argument);
    std::string value;
    if (separator != std::string::npos) value = argument.substr(separator + 1);
    else if ((key == "-v" || key == "--verbose") &&
             (index + 1 == input.size() || input[index + 1].starts_with('-')))
      value = "1";
    else if (index + 1 < input.size()) value = input[++index];
    if (value.empty()) return std::unexpected("missing watch option value: " + key);
    if (database) result.databases.push_back(watch::absolute(value, settings));
    else { result.arguments.push_back(key); result.arguments.push_back(value); }
  }
  return result;
}

std::vector<std::string> arguments(const Settings &settings, const Catalog &catalog,
                                  const Options &options, bool importing) {
  std::vector<std::string> result{importing ? "import" : "extract"};
  for (std::size_t index = 0; index + 1 < settings.defaults.size(); index += 2) {
    const auto &key = settings.defaults[index];
    bool overridden = key == "--conf";
    for (std::size_t offset = 0; offset < options.arguments.size(); offset += 2)
      overridden = overridden || options.arguments[offset] == key;
    if (overridden) continue;
    result.insert(result.end(), {key, settings.defaults[index + 1]});
  }
  result.insert(result.end(), options.arguments.begin(), options.arguments.end());
  result.insert(result.end(), {"--conf", catalog.database.string(), "--no-ast-cache"});
  if (!importing) result.push_back("--force");
  return result;
}
}

namespace facts::apis::watch {
std::expected<std::vector<std::filesystem::path>, std::string>
explicitDatabases(const Settings &settings) {
  return plan::options(settings, true).transform([](plan::Options options) {
    return std::move(options.databases);
  });
}
}
