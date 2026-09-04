#include "commands/catalog/Commands.h"
#include "commands/catalog/SymbolData.h"
#include "commands/catalog/SymbolFormat.h"
#include "commands/ConfigurationSupport.h"
#include "ui/symbol/SymbolBrowser.h"

#include <iostream>
#include <optional>
#include <utility>

namespace facts::commands {
namespace {

catalog::Result<int> renderScriptOutput(const cli::SymbolOptions &options,
                                        const std::vector<SymbolFact> &values) {
  if (options.action == cli::SymbolOptions::Action::list) {
    std::cout << displaySymbols(values);
    return 0;
  }
  if (values.empty())
    return std::unexpected("symbol '" + options.qualifiedName + "' not found");
  for (const auto &value : values)
    std::cout << displaySymbol(value);
  return 0;
}

} // namespace

catalog::Result<int> runSymbol(const cli::SymbolOptions &options) {
  auto configured = options;
  if (!options.configuration.empty() || !options.configurationFile.empty()) {
    auto resolved = loadConfiguration(options.configuration,
                                      options.configurationFile, false);
    if (!resolved) return std::unexpected(resolved.error());
    configured.configuration = resolved->database.string();
  }
  const auto name = configured.action == cli::SymbolOptions::Action::show
                        ? std::optional{configured.qualifiedName}
                        : std::nullopt;
  return loadSymbols(configured.facts, configured.configuration, name)
      .and_then([&](auto values) -> catalog::Result<int> {
        if (configured.action == cli::SymbolOptions::Action::browser)
          return ui::symbol::runBrowser(std::move(values));
        return renderScriptOutput(configured, values);
      });
}

} // namespace facts::commands
