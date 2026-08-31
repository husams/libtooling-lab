#pragma once

#include "storage/catalog/Requests.h"
#include <string>
#include <vector>

namespace facts::cli {

using CatalogSelector = catalog::Selector;

struct RepositoryOptions {
  enum class Action { list, show, addClone, switchClone, removeClone, remove };
  int verbosity = 0;
  std::string configuration;
  Action action = Action::list;
  std::string name;
  std::string path;
  std::string label;
  bool deleteComponents = false;
  bool dryRun = false;
};

struct FileOptions {
  enum class Action { add, remove, list, show, setOption, clearOption };
  int verbosity = 0;
  std::string configuration;
  Action action = Action::list;
  std::string path;
  std::string driver;
  std::string workingDirectory;
  std::string match;
  std::vector<std::string> arguments;
};

struct SymbolOptions {
  enum class Action { list, show, browser };
  int verbosity = 0;
  std::string facts;
  std::string configuration;
  Action action = Action::list;
  std::string qualifiedName;
};

struct ComponentOptions {
  enum class Action { list, show, add, setVersion, compileCommands, remove };
  int verbosity = 0;
  std::string configuration;
  Action action = Action::list;
  CatalogSelector selector;
  std::string repository;
  std::string kind = "repo";
  std::string version;
  bool noGit = false;
  bool dryRun = false;
};

struct DirectoryOptions {
  enum class Action { list, remove };
  int verbosity = 0;
  std::string configuration;
  Action action = Action::list;
  CatalogSelector selector;
  std::string component;
  bool dryRun = false;
};

} // namespace facts::cli
