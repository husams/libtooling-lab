#include "cli/CallGraphCommandLine.h"

#include <CLI/CLI.hpp>
#include <limits>

namespace facts::cli {

void configureCallGraphOptions(CLI::App &command, CallGraphOptions &options) {
  command
      .add_option_function<std::string>(
          "--component",
          [&](const std::string &name) { options.components.push_back(name); },
          "Include an explicitly named project component; repeatable")
      ->trigger_on_parse()
      ->type_name("NAME");
  command
      .add_option("--calls-scope", options.callsScope,
                  "Endpoint scope: all, project, or library")
      ->check(CLI::IsMember({"all", "project", "library"}))
      ->type_name("SCOPE");
  command
      .add_option("--max-depth", options.maxDepth,
                  "Maximum positive call-edge hop depth")
      ->check(CLI::Range(1, std::numeric_limits<int>::max()))
      ->type_name("N");
  command
      .add_option("--max-nodes", options.maxNodes,
                  "Maximum positive distinct canonical node count")
      ->check(CLI::PositiveNumber)
      ->type_name("N");
  command
      .add_option("--max-edges", options.maxEdges,
                  "Maximum positive distinct canonical edge count")
      ->check(CLI::PositiveNumber)
      ->type_name("N");
  command
      .add_option("--time-limit-ms", options.timeLimitMs,
                  "Positive monotonic traversal time limit in milliseconds")
      ->check(CLI::PositiveNumber)
      ->type_name("N");
}

} // namespace facts::cli
