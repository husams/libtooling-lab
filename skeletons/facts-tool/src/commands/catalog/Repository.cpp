#include "storage/catalog/Repository.h"
#include "commands/catalog/Commands.h"
#include "commands/catalog/Display.h"
#include "commands/catalog/Run.h"
#include "storage/catalog/Component.h"
#include <algorithm>
#include <format>

namespace facts::commands {
namespace {

catalog::Result<std::string> showRepository(catalog::Database &database,
                                            const catalog::Repository &repo) {
  return catalog::clones(database, repo.id).and_then([&](const auto &clones) {
    return catalog::components(database).and_then([&](auto components) {
      std::erase_if(components, [&](const auto &value) {
        return value.value.repositoryId != repo.id;
      });
      return displayComponents(components).transform([&](auto output) {
        std::string heading =
            displayRepositories({repo}) + "Clones (* active):\n";
        for (const auto &clone : clones) {
          heading += std::format("{} {}\t{}\t{}\n",
                                 repo.activeCloneId == clone.id ? '*' : ' ',
                                 clone.id, clone.label, clone.path);
        }
        return heading + output;
      });
    });
  });
}

catalog::Result<std::string> operate(catalog::Database &database,
                                     const cli::RepositoryOptions &options) {
  using Action = cli::RepositoryOptions::Action;
  if (options.action == Action::list) {
    return catalog::repositories(database).transform(displayRepositories);
  }
  return catalog::repository(database, options.name)
      .and_then([&](const catalog::Repository &repo)
                    -> catalog::Result<std::string> {
        switch (options.action) {
        case Action::show:
          return showRepository(database, repo);
        case Action::addClone:
          return catalog::addClone(database, repo, options.path, options.label)
              .transform([] { return std::string{"Clone registered\n"}; });
        case Action::switchClone:
          return catalog::switchClone(database, repo, options.path)
              .transform([] { return std::string{"Active clone switched\n"}; });
        case Action::removeClone:
          return catalog::removeClone(database, repo, options.path)
              .transform([] { return std::string{"Clone removed\n"}; });
        case Action::remove:
          if (options.dryRun)
            return "Would remove repository '" + repo.name + "'\n";
          return catalog::removeRepository(database, repo,
                                           options.deleteComponents)
              .transform([] { return std::string{"Repository removed\n"}; });
        case Action::list:
          break;
        }
        return std::unexpected("unsupported repository action");
      });
}
} // namespace

catalog::Result<int> runRepository(const cli::RepositoryOptions &options) {
  using Action = cli::RepositoryOptions::Action;
  const bool writable = !options.dryRun && options.action != Action::list &&
                        options.action != Action::show;
  return runCatalog(options.configuration, writable,
                    [&](auto &database) { return operate(database, options); });
}
} // namespace facts::commands
