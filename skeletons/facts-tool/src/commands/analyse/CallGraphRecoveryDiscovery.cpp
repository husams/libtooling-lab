#include "commands/analyse/CallGraphRecoverySelection.h"
#include "tooling/CompilationCommandCodec.h"
#include <algorithm>
#include <queue>
#include <ranges>

namespace facts::commands {
std::vector<FileId> indexedFiles(const RecoveryContext &context,
                                 std::string_view usr) {
  std::vector<FileId> result;
  if (const auto found = context.index.find(std::string{usr});
      found != context.index.end()) {
    for (const auto id : found->second) {
      if (context.commands.contains(id))
        result.push_back(id);
      else if (context.files.contains(id)) {
        std::set<FileId> seen{id};
        std::queue<FileId> pending;
        pending.push(id);
        while (!pending.empty()) {
          const auto current = pending.front();
          pending.pop();
          if (const auto found = context.includes.find(current);
              found != context.includes.end())
            for (const auto source : found->second) {
              if (!seen.insert(source).second)
                continue;
              pending.push(source);
              if (context.commands.contains(source))
                result.push_back(source);
            }
        }
      }
    }
  }
  if (result.empty())
    return fallbackFiles(context, 0);
  std::ranges::sort(result);
  result.erase(std::ranges::unique(result).begin(), result.end());
  return result;
}

std::vector<FileId> fallbackFiles(const RecoveryContext &context,
                                  FileId declarationFile) {
  std::vector<FileId> result;
  for (const auto &[id, command] : context.commands)
    result.push_back(id);
  return result;
}
} // namespace facts::commands
