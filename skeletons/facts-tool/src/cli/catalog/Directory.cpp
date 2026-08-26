#include "cli/catalog/Common.h"
#include "cli/catalog/Configure.h"

namespace facts::cli {

CLI::App *configureDirectory(CLI::App &app, DirectoryOptions &options) {
  using Action = DirectoryOptions::Action;
  auto *group = app.add_subcommand("dir", "Manage indexed directories");
  group->require_subcommand(1, 1);
  auto &list =
      catalogLeaf(*group, "list", "List directories", options, Action::list);
  list.alias("ls");
  list.add_option("-c,--component", options.component);
  auto &remove =
      catalogLeaf(*group, "rm", "Remove one directory and its catalog files",
                  options, Action::remove);
  catalogSelector(remove, options.selector, false);
  remove.add_option("-c,--component", options.component);
  remove.add_flag("--dry-run", options.dryRun);
  return group;
}

} // namespace facts::cli
