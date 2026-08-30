#include "cli/catalog/Common.h"
#include "cli/catalog/Configure.h"

namespace facts::cli {
namespace {

CLI::Option *arguments(CLI::App &command, FileOptions &options,
                       const char *description) {
  return command
      .add_option_function<std::string>(
          "--arg",
          [&options](const std::string &argument) {
            options.arguments.push_back(argument);
          },
          description)
      ->trigger_on_parse()
      ->type_name("TOKEN");
}

} // namespace

CLI::App *configureFile(CLI::App &app, FileOptions &options) {
  using Action = FileOptions::Action;
  auto *group = &catalogGroup(app, "file", "Manage registered files", options);
  catalogLeaf(*group, "list", "List registered files", options, Action::list)
      .alias("ls");
  catalogLeaf(*group, "show", "Show one registered file", options, Action::show)
      .add_option("path", options.path)
      ->required();

  auto &add = catalogLeaf(*group, "add", "Register one source file", options,
                          Action::add);
  add.add_option("path", options.path)->required();
  add.add_option("--driver", options.driver, "Compiler driver executable")
      ->required();
  add.add_option("--working-directory", options.workingDirectory,
                 "Compilation working directory");
  arguments(add, options, "Exact compile argument; repeatable");

  auto &remove = catalogLeaf(*group, "rm", "Remove one catalog file", options,
                             Action::remove);
  remove.alias("remove");
  remove.add_option("path", options.path)->required();

  auto configureOptionEdit = [&](const char *name, const char *description,
                                 Action action) {
    auto &leaf = catalogLeaf(*group, name, description, options, action);
    leaf.add_option("--match", options.match,
                    "Case-sensitive ECMAScript regular expression")
        ->required();
    arguments(leaf, options, "Exact contiguous argument token; repeatable")
        ->required();
  };
  configureOptionEdit("set-option", "Replace one exact option sequence",
                      Action::setOption);
  configureOptionEdit("clear-option", "Remove one exact option sequence",
                      Action::clearOption);
  return group;
}

} // namespace facts::cli
