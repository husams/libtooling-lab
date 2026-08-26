#include "cli/catalog/Common.h"
#include "cli/catalog/Configure.h"

namespace facts::cli {

CLI::App *configureRepository(CLI::App &app, RepositoryOptions &options) {
  using Action = RepositoryOptions::Action;
  auto *group =
      app.add_subcommand("repo", "Manage repositories and checkout clones");
  group->require_subcommand(1, 1);
  catalogLeaf(*group, "list", "List repositories", options, Action::list)
      .alias("ls");
  catalogLeaf(*group, "show", "Show clones and components", options,
              Action::show)
      .add_option("name", options.name)
      ->required();
  auto &add = catalogLeaf(*group, "add-clone", "Register a checkout", options,
                          Action::addClone);
  add.add_option("name", options.name)->required();
  add.add_option("path", options.path)->required();
  add.add_option("--label", options.label);
  auto &change = catalogLeaf(*group, "switch", "Switch the active checkout",
                             options, Action::switchClone);
  change.add_option("name", options.name)->required();
  change.add_option("target", options.path, "Registered clone path or label")
      ->required();
  auto &remove = catalogLeaf(*group, "rm",
                             "Remove repository metadata; keep checkout files",
                             options, Action::remove);
  remove.add_option("name", options.name)->required();
  remove.add_flag("--delete-components", options.deleteComponents,
                  "Also remove catalog components and files");
  remove.add_flag("--dry-run", options.dryRun,
                  "Preview without changing the database");
  return group;
}

} // namespace facts::cli
