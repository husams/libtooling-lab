#include "apis/watch/plan/Details.h"
#include "tooling/StoredCompilationReader.h"
#include <clang/Tooling/JSONCompilationDatabase.h>

namespace facts::apis::watch::plan {
std::expected<std::vector<Compilation>, std::string>
compilations(const std::vector<std::filesystem::path> &directories) {
  std::vector<Compilation> result;
  for (const auto &directory : std::set(directories.begin(), directories.end())) {
    std::string error;
    auto database = clang::tooling::JSONCompilationDatabase::loadFromFile(
        (directory / "compile_commands.json").string(), error,
        clang::tooling::JSONCommandLineSyntax::AutoDetect);
    if (!database) return std::unexpected("watch cannot load compilation database: " + error);
    Compilation compilation{directory, {}};
    for (const auto &command : database->getAllCompileCommands()) {
      auto working = std::filesystem::path(command.Directory);
      if (working.is_relative()) working = directory / working;
      auto source = std::filesystem::path(command.Filename);
      if (source.is_relative()) source = working / source;
      compilation.sources.push_back(source.lexically_normal().string());
    }
    result.push_back(std::move(compilation));
  }
  return result;
}

std::expected<std::vector<std::string>, std::string>
storedSources(const Catalog &catalog) {
  if (catalog.clones.empty()) return std::vector<std::string>{};
  return openStoredDatabase(catalog.database).and_then([](StoredDatabase database) {
    return readStoredCompilation(database.get()).transform([](auto snapshot) {
      std::vector<std::string> sources;
      for (const auto &file : snapshot.files) sources.push_back(file.path.string());
      return sources;
    });
  });
}
}
