#include "apis/watch/State.h"
#include "apis/watch/Arguments.h"
#include <algorithm>
#include <iterator>

namespace facts::apis {
namespace {
bool supplied(const std::vector<std::string> &values, std::string option) {
  if (option == "-c") option = "--conf";
  const auto end = std::find(values.begin(), values.end(), "--");
  return std::any_of(values.begin(), end, [&](const auto &value) {
    return value == option || value.starts_with(option + "=") ||
           (option == "--conf" && value.starts_with("-c"));
  });
}
}

std::vector<std::string> Watcher::Impl::arguments(
    std::string command, std::vector<std::string> values) const {
  watch::enableFlag(values, "--no-ast-cache");
  std::vector<std::string> result{std::move(command)};
  for (std::size_t i = 0; i < settings.defaults.size(); ++i) {
    const auto &option = settings.defaults[i];
    const auto key = option.substr(0, option.find('='));
    const bool paired = (key == "--config" || key == "--conf" || key == "-c" ||
                         key == "--facts" || key == "-f") &&
                        option.find('=') == std::string::npos &&
                        i + 1 < settings.defaults.size();
    if (!supplied(values, key)) {
      result.push_back(option);
      if (paired) result.push_back(settings.defaults[i + 1]);
    }
    if (paired) ++i;
  }
  result.insert(result.end(), std::make_move_iterator(values.begin()),
                std::make_move_iterator(values.end()));
  return result;
}

void Watcher::Impl::prepareImports() {
  imports.clear();
  if (!settings.importArguments.empty()) {
    imports.push_back(arguments("import", settings.importArguments));
    return;
  }
  for (const auto &directory : compilationDirectories)
    imports.push_back(arguments("import", {"-p", directory.string()}));
}
}
