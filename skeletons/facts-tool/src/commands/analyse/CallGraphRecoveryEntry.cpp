#include "commands/analyse/CallGraphRecoverySelection.h"
#include "commands/analyse/RecoveryCompilation.h"
#include "platform/PlatformFlags.h"
#include "tooling/CompilationCommandCodec.h"
#include <algorithm>
#include <clang/Tooling/ArgumentsAdjusters.h>

namespace facts::commands {
RecoveryEntry makeEntry(const RecoveryContext &context, FileId id,
                        std::vector<std::string> usrs, std::string reason) {
  RecoveryEntry entry;
  entry.tuFileId = id;
  entry.relatedUsrs = std::move(usrs);
  entry.reason = std::move(reason);
  if (const auto found = context.files.find(id); found != context.files.end()) {
    entry.component = found->second.componentName;
    entry.driver = found->second.driver;
    entry.workingDirectory = found->second.workingDirectory;
  }
  if (const auto found = context.commands.find(id);
      found != context.commands.end()) {
    entry.driver = found->second.driver;
    if (auto command = decodeStoredCommand(found->second, context.aliases)) {
      const std::vector<std::string> sources{found->second.path.string()};
      RecoveryCompilation stored(*command);
      auto configured = configurePlatformCompilationDatabase(stored, sources);
      if (!configured) {
        entry.reason = configured.error();
        return entry;
      }
      auto effective =
          (*configured)->getCompileCommands(sources.front()).front();
      auto arguments = clang::tooling::getClangStripOutputAdjuster()(
          effective.CommandLine, sources.front());
      arguments = clang::tooling::getClangSyntaxOnlyAdjuster()(arguments,
                                                               sources.front());
      entry.arguments = clang::tooling::getClangStripDependencyFileAdjuster()(
          arguments, sources.front());
      entry.workingDirectory = effective.Directory;
    } else {
      entry.reason = command.error();
    }
  }
  return entry;
}
} // namespace facts::commands
