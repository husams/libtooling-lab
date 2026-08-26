#include "cli/catalog/Common.h"
#include "cli/catalog/Configure.h"

namespace facts::cli {

CLI::App *configureComponent(CLI::App &app, ComponentOptions &options) {
  using Action = ComponentOptions::Action;
  auto *group = app.add_subcommand("component", "Manage project components");
  group->require_subcommand(1, 1);
  catalogLeaf(*group, "list", "List components", options, Action::list)
      .alias("ls");
  catalogLeaf(*group, "show", "Show a component", options, Action::show)
      .add_option("name", options.selector.name)
      ->required();
  auto &add =
      catalogLeaf(*group, "add", "Register a component", options, Action::add);
  add.add_option("--path", options.selector.path)
      ->required()
      ->check(CLI::ExistingDirectory);
  add.add_option("--name", options.selector.name);
  add.add_option("--repo", options.repository,
                 "Repository name (default: component name for repo kind)");
  add.add_option("--kind", options.kind)
      ->check(CLI::IsMember({"repo", "external"}));
  add.add_option("--version", options.version);
  add.add_flag("--no-git", options.noGit,
               "Do not promote the path to its enclosing Git root");
  auto &version =
      catalogLeaf(*group, "set-version", "Set a version; omit to clear",
                  options, Action::setVersion);
  version.add_option("name", options.selector.name)->required();
  version.add_option("version", options.version);
  catalogLeaf(*group, "compile-commands", "Export stored commands as JSON",
              options, Action::compileCommands)
      .add_option("name", options.selector.name)
      ->required();
  auto &remove =
      catalogLeaf(*group, "rm", "Remove a component's catalog entries", options,
                  Action::remove);
  catalogSelector(remove, options.selector, true);
  remove.add_flag("--dry-run", options.dryRun);
  return group;
}

} // namespace facts::cli
