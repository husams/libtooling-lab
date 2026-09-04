#include "commands/Configuration.h"
#include "config/Configuration.h"
#include <iostream>

namespace facts::commands {
std::expected<int, std::string>
runConfiguration(const cli::ConfigOptions &options) {
  auto value = config::resolve({options.configurationFile, options.direct, false});
  if (!value)
    return std::unexpected("facts-tool: configuration error: " + value.error());
  std::cout << "project_root: " << value->projectRoot << '\n';
  std::cout << "conf: " << value->database << '\n';
  std::cout << "conf_root: " << value->storageRoot << '\n';
  std::cout << "conf_template: " << value->templateText << '\n';
  std::cout << "source: " << value->source << '\n';
  std::cout << "conf_root_source: " << value->storageRootSource << '\n';
  std::cout << "conf_template_source: " << value->templateSource << '\n';
  std::cout << "extra_args_source: " << value->extraArgumentsSource << '\n';
  std::cout << "extra_args:";
  for (const auto &argument : value->extraArguments) std::cout << " [" << argument << ']';
  std::cout << '\n';
  std::cout << "discovery:\n";
  for (const auto &candidate : value->discovery) std::cout << "- " << candidate << '\n';
  return 0;
}
}
