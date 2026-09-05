#include "Sandbox.h"

namespace configuration_test {
// Anchored containment and substituted-value safety (B-030 C-3112/C-3114/
// C-3115/C-3117): every accepted path form is checked canonically against
// its own anchor, and every substituted value is checked for NUL/newline/
// backslash.
void safety() {
  Sandbox box;
  facts::config::Resolved v;
  v.projectRoot = box.root;
  v.storageRoot = box.root / "store";
  // An absolute literal template is accepted as written.
  v.templateText = (box.root / "literal/{filename}.db").string();
  auto literal = facts::config::renderDatabasePath(v);
  assert(literal && *literal == box.root / "literal" / (box.root.filename().string() + ".db"));
  v.templateText = "/outside.db";
  assert(facts::config::renderDatabasePath(v));
  // "~/" anchors to HOME: a later symlink escaping HOME is rejected.
  const auto outside = box.root.parent_path() / "facts-safety-outside";
  fs::create_directories(outside);
  fs::create_directory_symlink(outside, box.root / "linked");
  v.templateText = "~/linked/{filename}.db";
  auto tilde = facts::config::renderDatabasePath(v);
  assert(!tilde && tilde.error().find("escapes HOME") != std::string::npos);
  v.templateText = "~/{project_name}.db";
  assert(*facts::config::renderDatabasePath(v) == box.root / (box.root.filename().string() + ".db"));
  facts::config::Resolved f;
  f.projectRoot = box.root;
  f.factsTemplate = "~/linked/{filename}.db";
  const auto source = (box.root / "src/a.cpp").string();
  fs::create_directories(box.root / "src");
  std::ofstream(source) << "";
  assert(!facts::config::renderFactsPath(f, {source}));
  fs::remove_all(outside);
  // Substituted {user}/{project_root}/{project_name} values are validated.
  const auto saved = std::getenv("USER") ? std::optional<std::string>(std::getenv("USER")) : std::nullopt;
  setenv("USER", "bad\\user", 1);
  v.templateText = "{user}.db";
  assert(!facts::config::renderDatabasePath(v));
  unsetenv("USER");
  setenv("LOGNAME", "bad\nlogin", 1);
  assert(!facts::config::renderDatabasePath(v));
  unsetenv("LOGNAME");
  if (saved) setenv("USER", saved->c_str(), 1);
  facts::config::Resolved bad;
  bad.projectRoot = box.root / "bad\\dir";
  bad.storageRoot = box.root / "store";
  for (const auto text : {"{project_root}/x.db", "{project_name}.db"}) {
    bad.templateText = text;
    assert(!facts::config::renderDatabasePath(bad));
  }
  f.factsTemplate = "{project_root}/{filename}.db";
  assert(!facts::config::renderFactsPath(bad, {source}));
  // A ".." component in a losing tier still fails the whole resolution.
  box.write(".facts-tool.yaml", "conf_template: '{filename}.db'");
  fs::create_directories(box.root / ".config/facts-tool");
  box.write(".config/facts-tool/config.yaml", "conf_template: '../outside.db'");
  auto overridden = facts::config::resolve({});
  assert(!overridden && overridden.error().find("conf_template") != std::string::npos);
  box.write(".config/facts-tool/config.yaml", "facts_template: 'x/../y.db'");
  assert(!facts::config::resolve({}));
}
}
