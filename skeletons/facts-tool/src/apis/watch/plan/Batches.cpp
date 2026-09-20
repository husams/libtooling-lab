#include "apis/watch/plan/Details.h"
#include <numeric>

namespace facts::apis::watch::plan {
std::expected<void, std::string> appendBatches(
    std::vector<std::vector<std::string>> &jobs,
    std::vector<std::string> arguments, const std::set<std::string> &sources) {
  constexpr std::size_t maxArguments = 128, maxBytes = 65536;
  arguments.push_back("--");
  const auto baseBytes = std::accumulate(arguments.begin(), arguments.end(), std::size_t{},
      [](std::size_t sum, const auto &value) { return sum + value.size() + 1; });
  if (arguments.size() >= maxArguments || baseBytes >= maxBytes)
    return std::unexpected("watch arguments exceed job limits");
  auto batch = arguments;
  auto bytes = baseBytes;
  for (const auto &source : sources) {
    if (baseBytes + source.size() + 1 > maxBytes)
      return std::unexpected("watched source path exceeds job limits");
    if (batch.size() == maxArguments || bytes + source.size() + 1 > maxBytes) {
      jobs.push_back(std::move(batch));
      batch = arguments;
      bytes = baseBytes;
    }
    bytes += source.size() + 1;
    batch.push_back(source);
  }
  if (batch.size() > arguments.size()) jobs.push_back(std::move(batch));
  return {};
}
}
