#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace facts {
struct ExecutionCommand {
  std::string source, directory;
  std::vector<std::string> arguments;
};
struct ExecutionTrace {
  std::string stage;
  std::vector<ExecutionCommand> commands;
};
inline thread_local ExecutionTrace *executionTrace = nullptr;
inline void traceStage(std::string_view command, std::string_view stage) {
  if (executionTrace) executionTrace->stage = std::string(command) + ": " + std::string(stage);
}
inline void traceCommand(std::string source, std::string directory,
                         std::vector<std::string> arguments) {
  if (executionTrace && executionTrace->commands.size() < 32)
    executionTrace->commands.push_back({std::move(source), std::move(directory), std::move(arguments)});
}
}
