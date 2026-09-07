#pragma once
#include "cli/catalog/Options.h"
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace facts::cli {
struct ExtractOptions {
  int verbosity = 0;
  std::string output;
  bool outputFromTemplate = false;
  bool outputProvided = false;
  std::string configuration;
  std::string configurationFile;
  std::vector<std::string> defaultExtraArguments;
  std::vector<std::string> extraArguments;
  bool extraArgumentsProvided = false;
  std::vector<std::string> sources;
};

struct ImportOptions {
  int verbosity = 0;
  std::string configuration;
  std::string configurationFile;
  std::string facts;
  bool factsProvided = false;
  std::vector<std::string> defaultExtraArguments;
  std::string compilationDatabase;
  std::vector<std::string> components;
  std::vector<std::string> extraArguments;
  bool extraArgumentsProvided = false;
  std::vector<std::string> sources;
};

struct DependencyOptions {
  int verbosity = 0;
  std::string output;
  bool outputFromTemplate = false;
  bool outputProvided = false;
  std::string configuration;
  std::string configurationFile;
  std::vector<std::string> defaultExtraArguments;
  std::vector<std::string> extraArguments;
  bool extraArgumentsProvided = false;
  std::vector<std::string> sources;
};

struct CallGraphOptions {
  int verbosity = 0;
  std::string facts;
  std::string configuration;
  std::string configurationFile;
  std::optional<std::string> function;
  bool all = false;
  std::optional<int> maxDepth;
  std::vector<std::string> components;
  std::string callsScope = "all";
  std::optional<std::uint64_t> maxNodes;
  std::optional<std::uint64_t> maxEdges;
  std::optional<std::uint64_t> timeLimitMs;
  std::string direction = "callees";
  std::optional<std::string> target;
  std::optional<std::string> pathMode;
  bool recoverMissing = false;
};

struct CallGraphEntryOptions {
  int verbosity = 0;
  std::string facts;
  std::string configuration;
  std::string configurationFile;
  std::string format = "text";
  std::string function;
};

struct MatchOptions {
  int verbosity = 0;
  std::string facts;
  bool factsProvided = false;
  std::string configuration;
  std::string configurationFile;
  std::vector<std::string> defaultExtraArguments;
  std::string matcher;
  std::optional<std::string> relationKind;
  bool captureSource = false;
  std::vector<std::string> sources;
};

struct ConfigOptions {
  int verbosity = 0;
  std::string configurationFile;
  std::string direct;
};

using Command =
    std::variant<ExtractOptions, ImportOptions, DependencyOptions,
                 CallGraphOptions, CallGraphEntryOptions, MatchOptions,
                 ConfigOptions, RepositoryOptions, ComponentOptions,
                 DirectoryOptions, FileOptions, SymbolOptions>;

} // namespace facts::cli
