#include "cli/catalog/Common.h"
#include "cli/catalog/Configure.h"

namespace facts::cli {

CLI::App *configureDirectory(CLI::App &app, DirectoryOptions &options) {
  using Action = DirectoryOptions::Action;
  auto *group =
      &catalogGroup(app, "dir", "Manage indexed directories", options);
  auto &list =
      catalogLeaf(*group, "list", "List directories", options, Action::list);
  list.alias("ls");
  list.add_option("--component", options.component);
  auto &remove =
      catalogLeaf(*group, "rm", "Remove one directory and its catalog files",
                  options, Action::remove);
  catalogSelector(remove, options.selector, false);
  remove.add_option("--component", options.component);
  remove.add_flag("--dry-run", options.dryRun);
  return group;
}

} // namespace facts::cli
