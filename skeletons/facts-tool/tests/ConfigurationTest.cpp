#include "commands/ConfigurationSupport.h"
#include "configuration/Sandbox.h"
#include <cassert>
#include <filesystem>
#include <unistd.h>

int main() {
  configuration_test::schema();
  configuration_test::paths();
  configuration_test::discovery();
  configuration_test::arguments();
  configuration_test::ownership();
  configuration_test::policies();
  configuration_test::placeholders();
  const auto unmarked = std::filesystem::temp_directory_path() /
                        ("facts-tool-unmarked-" + std::to_string(getpid()));
  std::filesystem::create_directories(unmarked / "nested");
  assert(facts::config::detail::projectRoot(unmarked / "nested") == unmarked /
         "nested");

  facts::config::Resolved value;
  value.projectRoot = "/work/acme.v2";
  value.storageRoot = "/tmp/facts";
  value.templateText = "{relative_path}/{filename}.sqlite";
  auto path = facts::config::renderDatabasePath(value);
  assert(path && *path == std::filesystem::weakly_canonical("/tmp/facts/work/acme.v2.sqlite"));

  value.templateText = "nested/{filename}.db";
  path = facts::config::renderDatabasePath(value);
  assert(path && *path == std::filesystem::weakly_canonical("/tmp/facts/nested/acme.v2.db"));
  value.templateText = "../escape.db";
  assert(!facts::config::renderDatabasePath(value));

  auto args = facts::commands::mergedArguments(
      {"-DNAME=value with spaces", "-include", "header.h"},
      {"-DVALUE=1 '-DOTHER=two words'"});
  assert(args);
  assert(args->size() == 5);
  assert((*args)[1] == "-include");
  assert((*args)[3] == "-DVALUE=1");
  assert((*args)[4] == "-DOTHER=two words");
  assert(!facts::commands::mergedArguments({}, {"'unterminated"}));
  std::filesystem::remove_all(unmarked);
}
