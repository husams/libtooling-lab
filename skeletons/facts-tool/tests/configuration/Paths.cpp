#include "Sandbox.h"

namespace configuration_test {
void paths() {
  Sandbox box;
  facts::config::Resolved v;
  v.storageRoot = box.root / "store";
  v.projectRoot = "/work/acme.v2";
  for (const auto &[format, suffix] : std::vector<std::pair<std::string, std::string>>{
      {"{relative_path}/{filename}.db", "work/acme.v2.db"},
      {"nested//./{filename}", "nested/acme.v2"},
      {"{filename}-{filename}.sqlite", "acme.v2-acme.v2.sqlite"},
      {"literal", "literal"}, {"space 空間/{filename}", "space 空間/acme.v2"}}) {
    v.templateText = format;
    const auto result = facts::config::renderDatabasePath(v);
    assert(result && *result == v.storageRoot / suffix);
  }
  for (const auto format : {"", "/", ".", "..", "../x", "x/../y", "/x",
                           "{}", "{wrong}", "{filename", "x}", "dir/", "x\\y", "x\ny"}) {
    v.templateText = format;
    assert(!facts::config::renderDatabasePath(v));
  }
  v.templateText = "{relative_path}/{filename}.db";
  for (const auto &[project, suffix] : std::vector<std::pair<std::string, std::string>>{
      {"/", "_root.db"}, {"/acme", "acme.db"}, {"/work/{filename}", "work/{filename}.db"},
      {"/space 空間/a.b", "space 空間/a.b.db"}}) {
    v.projectRoot = project;
    auto result = facts::config::renderDatabasePath(v);
    assert(result && *result == v.storageRoot / suffix);
  }
  fs::create_directories(v.storageRoot);
  fs::create_directories(box.root / "outside");
  fs::create_directory_symlink(box.root / "outside", v.storageRoot / "link");
  v.templateText = "link/escaped";
  assert(!facts::config::renderDatabasePath(v));
}
}
