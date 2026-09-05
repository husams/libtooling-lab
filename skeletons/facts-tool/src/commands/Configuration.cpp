#include "commands/Configuration.h"
#include "config/Configuration.h"
#include <iostream>

namespace facts::commands {
std::expected<int, std::string>
runConfiguration(const cli::ConfigOptions &options) {
  config::Resolved partial;
  auto result = config::resolve({options.configurationFile, options.direct, false}, &partial);
  const auto *value = result ? &*result : &partial;
  std::cout << "parser: YAML / yaml-cpp 0.9.0\n";
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
  if (!result)
    return std::unexpected("facts-tool: configuration error: " + result.error());
  return 0;
}
}
