#include "commands/ConfigurationSupport.h"
#include <cassert>
#include <filesystem>

int main() {
  facts::config::Resolved value;
  value.projectRoot = "/work/acme.v2";
  value.storageRoot = "/tmp/facts";
  value.templateText = "{relative_path}/{filename}.sqlite";
  auto path = facts::config::renderDatabasePath(value);
  assert(path && path->string() == "/tmp/facts/work/acme.v2.sqlite");

  value.templateText = "nested/{filename}.db";
  path = facts::config::renderDatabasePath(value);
  assert(path && path->string() == "/tmp/facts/nested/acme.v2.db");
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
}
