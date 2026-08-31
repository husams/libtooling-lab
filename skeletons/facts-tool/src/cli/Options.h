#pragma once

#include "cli/catalog/Options.h"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace facts::cli {

struct ExtractOptions {
  int verbosity = 0;
  std::string output;
  std::string configuration;
  std::vector<std::string> extraArguments;
  std::vector<std::string> sources;
};

struct ImportOptions {
  int verbosity = 0;
  std::string configuration;
  std::string compilationDatabase;
  std::vector<std::string> components;
  std::vector<std::string> extraArguments;
  std::vector<std::string> sources;
};

struct DependencyOptions {
  int verbosity = 0;
  std::string output;
  std::string configuration;
  std::vector<std::string> extraArguments;
  std::vector<std::string> sources;
};

struct CallGraphOptions {
  int verbosity = 0;
  std::string facts;
  std::optional<std::string> function;
  bool all = false;
  std::optional<int> maxDepth;
};

using Command =
    std::variant<ExtractOptions, ImportOptions, DependencyOptions,
                 CallGraphOptions, RepositoryOptions, ComponentOptions,
                 DirectoryOptions, FileOptions, SymbolOptions>;

} // namespace facts::cli
