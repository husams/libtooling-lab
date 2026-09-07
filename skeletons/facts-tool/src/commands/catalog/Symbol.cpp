#include "commands/ConfigurationSupport.h"
#include "commands/catalog/Commands.h"
#include "commands/catalog/MatchedSymbolFormat.h"
#include "commands/catalog/MatchedSymbolQuery.h"
#include "commands/catalog/Run.h"
#include "commands/catalog/SymbolData.h"
#include "commands/catalog/SymbolFormat.h"
#include "ui/symbol/SymbolBrowser.h"

#include <iostream>
#include <optional>
#include <utility>

namespace facts::commands {
namespace {

catalog::Result<int> runMatchedIndex(const cli::SymbolOptions &options) {
  using Action = cli::SymbolOptions::Action;
  const bool clear = options.action == Action::clearIndex;
  return runCatalog(
      options.configuration, clear,
      [&](catalog::Database &database) -> catalog::Result<std::string> {
        if (clear)
          return clearMatchedSymbols(database,
                                     static_cast<FileId>(options.fileId))
              .transform([](std::size_t count) {
                return "Cleared " + std::to_string(count) +
                       " matched symbol candidate(s)\n";
              });
        return findMatchedSymbols(database, options.usr, options.name,
                                  options.kind)
            .transform([&](const auto &values) {
              return options.format == "json"
                         ? renderMatchedSymbolsJson(values)
                         : renderMatchedSymbolsText(values);
            });
      },
      false, options.configurationFile, options.facts);
}

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
  if (options.action == cli::SymbolOptions::Action::find ||
      options.action == cli::SymbolOptions::Action::clearIndex)
    return runMatchedIndex(options);
  auto configured = options;
  // An explicit --facts with no configuration flags stays independent of
  // configuration (S-019): no discovery happens at all. Otherwise resolve,
  // both to locate the project conf DB and to fill a missing --facts from
  // facts_template.
  const bool needsConfiguration = !options.factsProvided ||
                                  !options.configuration.empty() ||
                                  !options.configurationFile.empty() ||
                                  config::detail::present("FACTS_TOOL_CONF");
  if (needsConfiguration) {
    auto resolved =
        loadConfiguration(options.configuration, options.configurationFile,
                          false, !options.factsProvided);
    if (!resolved)
      return std::unexpected(resolved.error());
    if (!options.configuration.empty() || !options.configurationFile.empty() ||
        config::detail::present("FACTS_TOOL_CONF")) {
      configured.configuration = resolved->database.string();
      if (!std::filesystem::exists(resolved->database))
        return std::unexpected("project configuration database not found: " +
                               configured.configuration);
    }
    if (!options.factsProvided) {
      auto facts = resolveFactsOutput(*resolved, {});
      if (!facts)
        return std::unexpected(facts.error());
      configured.facts = facts->string();
    }
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
