#include "Sandbox.h"
#include "commands/ConfigurationSupport.h"

namespace configuration_test {
namespace {
fs::path userFile(Sandbox &box) {
  return box.root / ".config/facts-tool/config.yaml";
}
}

void discovery() {
  Sandbox box;
  auto builtin = facts::config::resolve({});
  assert(builtin && builtin->projectRoot == box.root);
  assert(builtin->storageRootSource == "built-in" &&
        builtin->templateSource == "built-in" &&
        builtin->factsTemplateSource == "built-in" &&
        builtin->extraArgumentsSource == "built-in");
  assert(builtin->discovery.size() == 2);

  // Project tier alone wins over the built-in default.
  box.write(".facts-tool.yaml", "conf_root: project-store\nconf_template: p.db\n"
                                "extra_args: [-DPROJECT]");
  auto project = facts::config::resolve({});
  assert(project && project->database == box.root / "project-store/p.db");
  assert(project->storageRootSource == (box.root / ".facts-tool.yaml").string());
  assert(project->extraArguments == std::vector<std::string>{"-DPROJECT"});

  // User tier alone (project absent) supplies its own values.
  fs::remove(box.root / ".facts-tool.yaml");
  fs::create_directories(userFile(box).parent_path());
  std::ofstream(userFile(box)) << "conf_root: user-store\nconf_template: u.db\n"
                                  "extra_args: [-DUSER]";
  auto user = facts::config::resolve({});
  assert(user && user->database == box.root / "user-store/u.db");
  assert(user->storageRootSource == userFile(box).string());
  assert(user->extraArguments == std::vector<std::string>{"-DUSER"});

  // Both present: project wins the scalar keys, extra_args concatenates
  // user then project.
  box.write(".facts-tool.yaml", "conf_root: project-store\nconf_template: p.db\n"
                                "extra_args: [-DPROJECT]");
  auto both = facts::config::resolve({});
  assert(both && both->database == box.root / "project-store/p.db");
  assert(both->storageRootSource == (box.root / ".facts-tool.yaml").string());
  assert(both->extraArguments == std::vector<std::string>({"-DUSER", "-DPROJECT"}));
  assert(both->extraArgumentsSource.find(userFile(box).string()) != std::string::npos &&
        both->extraArgumentsSource.find((box.root / ".facts-tool.yaml").string()) !=
            std::string::npos);
  assert(both->discovery.size() == 2);

  // An explicit --config file outranks the project file for scalars and is
  // appended last for extra_args.
  const auto explicitYaml = box.write("selected.yaml",
      "conf_root: cli-store\nconf_template: c.db\nextra_args: [-DCLI]");
  auto selected = facts::config::resolve({explicitYaml.string()});
  assert(selected && selected->database == box.root / "cli-store/c.db");
  assert(selected->storageRootSource == explicitYaml.string());
  assert(selected->extraArguments ==
        std::vector<std::string>({"-DUSER", "-DPROJECT", "-DCLI"}));
  assert(selected->discovery.size() == 3);

  setenv("FACTS_TOOL_CONFIG", explicitYaml.c_str(), 1);
  assert(facts::config::resolve({})->database == selected->database);
  setenv("FACTS_TOOL_CONF", "env.db", 1);
  assert(facts::config::resolve({})->database == box.root / "env.db");
  assert(facts::config::resolve({"", "cli.db"})->database == box.root / "cli.db");
  unsetenv("FACTS_TOOL_CONFIG");

  // A direct override still merges extra_args from valid tiers even though
  // it bypasses conf_root/conf_template rendering entirely.
  const auto direct = facts::config::resolve({"", "cli.db"});
  assert(direct && direct->extraArguments ==
        std::vector<std::string>({"-DUSER", "-DPROJECT"}));

  box.write(".facts-tool.yaml", "conf_root: [bad");
  for (const bool compiler : {false, true}) {
    const auto resolved = facts::commands::loadConfiguration("direct.db", "", false, compiler);
    assert(static_cast<bool>(resolved) == !compiler);
  }
  fs::remove(box.root / ".facts-tool.yaml");
  unsetenv("FACTS_TOOL_CONF");
  fs::remove(userFile(box));
  unsetenv("HOME");
  assert(!facts::config::resolve({}));
  setenv("XDG_DATA_HOME", box.root.c_str(), 1);
  assert(facts::config::resolve({}));
  setenv("XDG_DATA_HOME", "relative", 1);
  assert(!facts::config::resolve({}));
  assert(facts::config::resolve({"", "direct.db"}));
}
}
