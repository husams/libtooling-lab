#include "storage/catalog/Directory.h"
#include "commands/catalog/Commands.h"
#include "commands/catalog/Display.h"
#include "commands/catalog/Run.h"

namespace facts::commands {

catalog::Result<int> runDirectory(const cli::DirectoryOptions &options) {
  const bool remove = options.action == cli::DirectoryOptions::Action::remove;
  return runCatalog(
      options.configuration, remove && !options.dryRun,
      [&](auto &database) -> catalog::Result<std::string> {
        if (!remove)
          return catalog::directories(database, options.component)
              .transform(displayDirectories);
        return catalog::directory(database, options.selector, options.component)
            .and_then([&](const catalog::Directory &value)
                          -> catalog::Result<std::string> {
              if (options.dryRun)
                return "Would remove directory:\n" +
                       displayDirectories({value});
              return catalog::removeDirectory(database, value.id).transform([] {
                return std::string{"Directory removed from catalog\n"};
              });
            });
      }, false, options.configurationFile);
}
} // namespace facts::commands
