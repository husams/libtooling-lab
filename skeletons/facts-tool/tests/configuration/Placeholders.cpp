#include "Sandbox.h"
#include "commands/ConfigurationSupport.h"
#include <pwd.h>
#include <unistd.h>

namespace configuration_test {
void placeholders() {
  Sandbox box;
  // A relative conf_root anchors to the project root, not to the directory
  // holding the YAML file that declared it (B-030).
  const auto nested = box.root / "nested";
  fs::create_directories(nested);
  box.write("nested/team.yaml", "conf_root: .index\nconf_template: '{filename}.db'");
  auto resolved = facts::config::resolve({(nested / "team.yaml").string()});
  assert(resolved && resolved->projectRoot == box.root);
  assert(*facts::config::renderDatabasePath(*resolved) ==
        box.root / ".index" / (box.root.filename().string() + ".db"));

  // Full placeholder set on conf_template: {project_root}, {project_name},
  // {user}, and ${ENV}.
  setenv("FACTS_TOOL_TEST_ENV", "envvalue", 1);
  facts::config::Resolved v;
  v.projectRoot = box.root;
  v.storageRoot = box.root / "store";
  v.templateText = "{project_name}/${FACTS_TOOL_TEST_ENV}/{user}.db";
  const auto user = getenv("USER") ? std::string(getenv("USER"))
                                   : std::string(getpwuid(geteuid())->pw_name);
  auto rendered = facts::config::renderDatabasePath(v);
  assert(rendered && *rendered ==
        v.storageRoot / box.root.filename() / "envvalue" / (user + ".db"));
  unsetenv("FACTS_TOOL_TEST_ENV_UNSET");
  v.templateText = "${FACTS_TOOL_TEST_ENV_UNSET}.db";
  assert(!facts::config::renderDatabasePath(v));

  // facts_template: absolute via {project_root}, single-source requirement,
  // and the usage-error path when the run is ambiguous.
  facts::config::Resolved f;
  f.projectRoot = box.root;
  f.factsTemplate = "{project_root}/.index/{relative_path}/{filename}.db";
  const auto source = (box.root / "src/a/b.cpp").string();
  fs::create_directories(box.root / "src/a");
  std::ofstream(source) << "";
  auto one = facts::config::renderFactsPath(f, {source});
  assert(one && *one == box.root / ".index/src/a/b.db");
  auto none = facts::config::renderFactsPath(f, {});
  assert(!none && none.error().starts_with("usage:"));
  auto many = facts::config::renderFactsPath(f, {source, source});
  assert(!many && many.error().starts_with("usage:"));
  f.factsTemplate = "{project_root}/.index/project.db";
  auto project = facts::config::renderFactsPath(f, {});
  assert(project && *project == box.root / ".index/project.db");
  facts::config::Resolved empty;
  empty.projectRoot = box.root;
  auto missing = facts::config::renderFactsPath(empty, {});
  assert(!missing && missing.error().starts_with("usage:"));
}
}
