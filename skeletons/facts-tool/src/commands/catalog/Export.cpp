#include "commands/catalog/Export.h"
#include "tooling/StoredCompilationDatabase.h"
#include <llvm/Support/JSON.h>
#include <llvm/Support/raw_ostream.h>

namespace facts::commands {

catalog::Result<std::string>
exportCommands(catalog::Database &database, const std::string &configuration,
               const catalog::Component &component) {
  return catalog::componentRoot(component).and_then([&](const auto &root) {
    return catalog::query(
               database,
               "SELECT d.path,f.name FROM file f JOIN directory d ON "
               "d.id=f.directory_id "
               "WHERE d.component_id=? AND f.compile_options IS NOT NULL ORDER "
               "BY f.id",
               [&](const storage::Row &row) {
                 return (root / row.string(0) / row.string(1)).string();
               },
               component.value.id)
        .and_then([&](const auto &files) {
          return loadStoredCompilationDatabase(configuration)
              .and_then([&](auto stored) -> catalog::Result<std::string> {
                llvm::json::Array output;
                for (const auto &file : files) {
                  const auto commands = stored->getCompileCommands(file);
                  if (commands.empty())
                    return std::unexpected(
                        "stored compile command not found: " + file);
                  for (const auto &command : commands) {
                    llvm::json::Array arguments;
                    for (const auto &argument : command.CommandLine)
                      arguments.push_back(argument);
                    output.push_back(llvm::json::Object{
                        {"directory", command.Directory},
                        {"file", command.Filename},
                        {"arguments", std::move(arguments)}});
                  }
                }
                std::string text;
                llvm::raw_string_ostream stream(text);
                stream << llvm::json::Value(std::move(output)) << '\n';
                return text;
              });
        });
  });
}
} // namespace facts::commands
