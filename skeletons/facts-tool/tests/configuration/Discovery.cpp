#include "Sandbox.h"
#include "commands/ConfigurationSupport.h"

namespace configuration_test {
void discovery() {
  Sandbox box;
  auto builtin = facts::config::resolve({});
  assert(builtin && builtin->projectRoot == box.root);
  assert(builtin->storageRootSource == "built-in");
  const auto explicitYaml = box.write("selected.yaml",
      "conf_root: local\nconf_template: chosen.db\nextra_args: [-DVALUE]");
  box.write(".facts-tool.yaml", "broken: [");
  auto selected = facts::config::resolve({explicitYaml.string()});
  assert(selected && selected->database == box.root / "local/chosen.db");
  assert(selected->storageRootSource == explicitYaml.string() + ": conf_root");
  assert(selected->discovery.size() == 3 && selected->discovery[1].ends_with("[skipped]"));
  setenv("FACTS_TOOL_CONFIG", explicitYaml.c_str(), 1);
  assert(facts::config::resolve({})->database == selected->database);
  setenv("FACTS_TOOL_CONF", "env.db", 1);
  assert(facts::config::resolve({})->database == box.root / "env.db");
  assert(facts::config::resolve({"", "cli.db"})->database == box.root / "cli.db");
  unsetenv("FACTS_TOOL_CONFIG");
  for (const bool compiler : {false, true}) {
    const auto resolved = facts::commands::loadConfiguration("direct.db", "", false, compiler);
    assert(static_cast<bool>(resolved) == !compiler);
  }
  unsetenv("FACTS_TOOL_CONF");
  box.write(".facts-tool.yaml", "{}");
  unsetenv("HOME");
  assert(!facts::config::resolve({}));
  setenv("XDG_DATA_HOME", box.root.c_str(), 1);
  assert(facts::config::resolve({}));
  setenv("XDG_DATA_HOME", "relative", 1);
  assert(!facts::config::resolve({}));
  assert(facts::config::resolve({"", "direct.db"}));
}
}
