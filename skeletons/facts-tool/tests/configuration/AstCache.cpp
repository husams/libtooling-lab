#include "Sandbox.h"

namespace configuration_test {
namespace {

void cacheSchema() {
  Sandbox box;
  for (const auto text : {
      "ast_cache: null", "ast_cache: 1", "ast_cache: yes", "ast_cache: on",
      "ast_cache: True", "ast_cache: 'true'", "ast_cache: []", "ast_cache: {}",
      "ast_cache: true\nast_cache: false", "ast_cache_dir: null",
      "ast_cache_dir: ''", "ast_cache_dir: []", "ast_cache_dir: {}",
      "ast_cache_dir: true", "ast_cache_dir: 123",
      R"(ast_cache_dir: "a\0b")", R"(ast_cache_dir: "a\nb")"}) {
    for (const bool generated : {false, true})
      assert(!facts::config::readTier(box.write("invalid.yaml", text), generated));
  }
  for (const bool generated : {false, true}) {
    auto value = facts::config::readTier(box.write("valid.yaml",
        "ast_cache: false\nast_cache_dir: 'a cache directory'"), generated);
    assert(value && value->astCache.has_value() && !*value->astCache);
    assert(value->astCacheDirectory == "a cache directory");
  }
}

void defaults() {
  Sandbox box;
  const auto value = facts::config::resolve({});
  assert(value && !value->astCache.enabled);
  assert(value->astCache.directory == box.root / ".facts-tool/ast-cache");
  assert(value->astCacheSource == "built-in");
  assert(value->astCacheDirectorySource == "built-in");
  assert(!fs::exists(value->astCache.directory));
}

void precedence() {
  Sandbox box;
  const auto user = box.write(".config/facts-tool/config.yaml",
      "ast_cache: true\nast_cache_dir: user-cache");
  auto value = facts::config::resolve({.direct = "project.db"});
  assert(value && value->astCache.enabled);
  assert(value->astCache.directory == box.root / "user-cache");
  assert(value->astCacheSource == user.string() && value->astCacheDirectorySource == user.string());

  const auto project = box.write(".facts-tool.yaml", "ast_cache: false");
  value = facts::config::resolve({.direct = "project.db"});
  assert(value && !value->astCache.enabled);
  assert(value->astCacheSource == project.string() && value->astCacheDirectorySource == user.string());

  const auto selected = box.write("team/config.yaml", "ast_cache_dir: selected-cache");
  value = facts::config::resolve({.selector = selected.string(), .direct = "project.db"});
  assert(value && !value->astCache.enabled);
  assert(value->astCache.directory == box.root / "selected-cache");
  assert(value->astCacheSource == project.string() && value->astCacheDirectorySource == selected.string());

  box.write("team/config.yaml", "ast_cache: true\nast_cache_dir: selected-cache");
  value = facts::config::resolve({.selector = selected.string(), .direct = "project.db"});
  assert(value && value->astCache.enabled && value->astCacheSource == selected.string());
  assert(!fs::exists(value->astCache.directory));
}

void cachePaths() {
  Sandbox box;
  box.write("project/.facts-tool.yaml", "ast_cache: true");
  const auto selected = box.write("team.yaml", "ast_cache_dir: custom-cache");
  fs::create_directories(box.root / "project/nested");
  fs::current_path(box.root / "project/nested");
  auto value = facts::config::resolve({.selector = selected.string(), .direct = "project.db"});
  assert(value && value->astCache.directory == box.root / "project/custom-cache");
  assert(!fs::exists(box.root / "project/custom-cache"));

  box.write("team.yaml", "ast_cache_dir: ~/user-cache");
  value = facts::config::resolve({.selector = selected.string(), .direct = "project.db"});
  assert(value && value->astCache.directory == box.root / "user-cache");
  box.write("team.yaml", "ast_cache_dir: '" + (box.root / "absolute-cache").string() + "'");
  value = facts::config::resolve({.selector = selected.string(), .direct = "project.db"});
  assert(value && value->astCache.directory == box.root / "absolute-cache");

  box.write("team.yaml", "ast_cache_dir: ~/user-cache");
  unsetenv("HOME");
  value = facts::config::resolve({.selector = selected.string(), .direct = "project.db"});
  assert(!value && value.error().find("ast_cache_dir in " + selected.string()) != std::string::npos);
  assert(value.error().find("requires HOME") != std::string::npos);
}

void invalidLowerTier() {
  Sandbox box;
  const auto user = box.write(".config/facts-tool/config.yaml", "ast_cache: yes");
  const auto selected = box.write("team.yaml", "ast_cache: true\nast_cache_dir: valid-cache");
  const auto value = facts::config::resolve({.selector = selected.string(), .direct = "project.db"});
  assert(!value && value.error().find("ast_cache in " + user.string()) != std::string::npos);
  assert(!fs::exists(box.root / "valid-cache"));
}

} // namespace

void astCache() {
  cacheSchema();
  defaults();
  precedence();
  cachePaths();
  invalidLowerTier();
}

} // namespace configuration_test
