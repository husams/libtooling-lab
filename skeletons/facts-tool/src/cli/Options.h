#pragma once

#include "cli/catalog/Options.h"

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

using Command =
    std::variant<ExtractOptions, ImportOptions, DependencyOptions,
                 RepositoryOptions, ComponentOptions, DirectoryOptions>;

} // namespace facts::cli
