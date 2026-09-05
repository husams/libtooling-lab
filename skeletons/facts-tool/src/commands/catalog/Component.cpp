#include "storage/catalog/Component.h"
#include "commands/catalog/Commands.h"
#include "commands/catalog/Display.h"
#include "commands/catalog/Export.h"
#include "commands/catalog/Run.h"

namespace facts::commands {
namespace {

catalog::Result<std::string> operate(catalog::Database &database,
                                     const cli::ComponentOptions &options) {
  using Action = cli::ComponentOptions::Action;
  if (options.action == Action::list) {
    return catalog::components(database).and_then(displayComponents);
  }
  if (options.action == Action::add) {
    return catalog::addComponent(database,
                                 {options.selector.name, options.selector.path,
                                  options.repository, options.kind,
                                  options.version, options.noGit})
        .transform([] { return std::string{"Component registered\n"}; });
  }
  return catalog::component(database, options.selector)
      .and_then(
          [&](const catalog::Component &value) -> catalog::Result<std::string> {
            switch (options.action) {
            case Action::show:
              return displayComponents({value});
            case Action::compileCommands:
              return exportCommands(database, options.configuration, value);
            case Action::setVersion:
              return catalog::setVersion(database, value, options.version)
                  .transform([] {
                    return std::string{"Component version updated\n"};
                  });
            case Action::remove:
              if (options.dryRun) {
                return displayComponents({value}).transform([](auto text) {
                  return "Would remove component:\n" + text;
                });
              }
              return catalog::removeComponent(database, value.value.id)
                  .transform([] {
                    return std::string{"Component removed from catalog\n"};
                  });
            case Action::list:
            case Action::add:
              break;
            }
            return std::unexpected("unsupported component action");
          });
}
} // namespace

catalog::Result<int> runComponent(const cli::ComponentOptions &options) {
  using Action = cli::ComponentOptions::Action;
  auto configured = options;
  if (options.action == Action::compileCommands) {
    auto resolved = loadConfiguration(options.configuration,
                                      options.configurationFile, false);
    if (!resolved) return std::unexpected(resolved.error());
    configured.configuration = resolved->database.string();
  }
  const bool writable =
      !configured.dryRun &&
      (configured.action == Action::add || configured.action == Action::remove ||
       configured.action == Action::setVersion);
  return runCatalog(
      configured.configuration, writable,
      [&](auto &database) { return operate(database, configured); },
      configured.action == Action::add, configured.configurationFile);
}
} // namespace facts::commands
