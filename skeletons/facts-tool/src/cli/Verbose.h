#pragma once

#include <format>
#include <functional>
#include <iostream>
#include <string_view>
#include <utility>

namespace facts::cli {

inline constexpr int maximumVerbosity = 2;

template <typename... Arguments>
void logVerbose(int configuredLevel, int requiredLevel,
                std::format_string<Arguments...> message,
                Arguments &&...arguments) {
  if (configuredLevel < requiredLevel) {
    return;
  }
  std::clog << std::format(message, std::forward<Arguments>(arguments)...)
            << '\n';
}

template <typename Operation>
decltype(auto) runStage(int verbosity, std::string_view command,
                        std::string_view stage, Operation &&operation) {
  logVerbose(verbosity, 1, "facts-tool: {}: {}", command, stage);
  return std::invoke(std::forward<Operation>(operation));
}

} // namespace facts::cli
