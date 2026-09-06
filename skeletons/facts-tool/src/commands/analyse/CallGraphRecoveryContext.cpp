#include "commands/analyse/CallGraphRecoveryInternal.h"

#include "commands/ConfigurationSupport.h"
#include "storage/catalog/File.h"
#include "tooling/CompilationCommandCodec.h"

namespace facts::commands {

std::expected<RecoveryContext, std::string>
loadRecoveryContext(const cli::CallGraphOptions &options) {
  if (options.configuration.empty() && options.configurationFile.empty())
    return std::unexpected("recovery requires a project configuration");
  return loadConfiguration(options.configuration, options.configurationFile,
                           false)
      .and_then([&](const auto &resolved) {
        auto opened = catalog::open(resolved.database.string(), false);
        if (!opened)
          return std::expected<RecoveryContext, std::string>{
              std::unexpected(opened.error())};
        auto files = catalog::files(*opened);
        if (!files)
          return std::expected<RecoveryContext, std::string>{
              std::unexpected(files.error())};
        auto stored = openStoredDatabase(resolved.database);
        if (!stored)
          return std::expected<RecoveryContext, std::string>{
              std::unexpected(stored.error())};
        return readStoredCompilation(stored->get()).and_then(
            [&](auto snapshot) {
              RecoveryContext context{resolved.database.string()};
              context.aliases = snapshot.labels;
              for (auto file : *files)
                context.files.emplace(file.id, std::move(file));
              for (auto &file : snapshot.files)
                context.commands.emplace(file.id, std::move(file));
              auto facts = storage::Database::open(options.facts,
                                                   storage::Database::readOnly);
              if (!facts)
                return std::expected<RecoveryContext, std::string>{
                    std::unexpected(facts.error().message())};
              {
                auto includes = catalog::query(
                    *facts, "SELECT src_file_id,dst_file_id FROM include_dependency",
                    [](const storage::Row &row) {
                      return std::pair<FileId, FileId>{row.get<FileId>(0),
                                                       row.get<FileId>(1)};
                    });
                if (includes)
                  for (const auto [source, destination] : *includes)
                    context.includes[destination].push_back(source);
                if (!includes && includes.error().find("no such table") ==
                                     std::string::npos)
                  return std::expected<RecoveryContext, std::string>{
                      std::unexpected(includes.error())};
              }
              auto rows = catalog::query(
                  *opened,
                  "SELECT usr,file_id FROM matched_symbol_index ORDER BY usr,file_id",
                  [](const storage::Row &row) {
                    return std::pair<std::string, FileId>{
                        row.string(0), row.get<FileId>(1)};
                  });
              if (!rows && rows.error().find("no such table") !=
                              std::string::npos)
                return std::expected<RecoveryContext, std::string>{
                    std::move(context)};
              if (!rows)
                return std::expected<RecoveryContext, std::string>{
                    std::unexpected(rows.error())};
              for (auto &[usr, file] : *rows)
                context.index[std::move(usr)].push_back(file);
              return std::expected<RecoveryContext, std::string>{
                  std::move(context)};
            });
      });
}

} // namespace facts::commands
