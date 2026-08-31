#include "commands/catalog/Commands.h"
#include "commands/catalog/SymbolData.h"
#include "commands/catalog/SymbolFormat.h"
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
  const auto name = options.action == cli::SymbolOptions::Action::show
                        ? std::optional{options.qualifiedName}
                        : std::nullopt;
  return loadSymbols(options.facts, options.configuration, name)
      .and_then([&](auto values) -> catalog::Result<int> {
        if (options.action == cli::SymbolOptions::Action::browser)
          return ui::symbol::runBrowser(std::move(values));
        return renderScriptOutput(options, values);
      });
}

} // namespace facts::commands
