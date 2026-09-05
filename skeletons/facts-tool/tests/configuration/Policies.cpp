#include "Sandbox.h"
#include "commands/ConfigurationSupport.h"

namespace configuration_test {
void policies() {
  for (const bool create : {false, true}) {
    for (const bool compiler : {false, true}) {
      Sandbox box;
      box.write(".facts-tool.yaml", "conf_root: store\nconf_template: owned.db");
      const auto value = facts::commands::loadConfiguration("", "", create, compiler);
      assert(value && fs::exists(value->database) == create);
      assert(fs::exists(box.root / "store") == create);
      box.write(".facts-tool.yaml", "extra_args: [null]");
      assert(!facts::commands::loadConfiguration("", "", create, compiler));
    }
  }
  for (const auto name : {"FACTS_TOOL_CONF", "FACTS_TOOL_CONFIG"}) {
    Sandbox box;
    setenv(name, "", 1);
    auto value = facts::config::resolve({});
    assert(!value && value.error().find("must not be empty") != std::string::npos);
  }
}
}
