#include "commands/FactPairCloneAliases.h"

namespace facts::commands::detail {
std::expected<CloneAliases, std::string>
loadCloneAliases(storage::Database &database, const std::vector<catalog::File> &files) {
  return catalog::query(database, "SELECT id,repository_id,path,label FROM clone",
      [](const storage::Row &row) {
        return ProjectClone{row.integer(0), row.integer(1), row.string(2), row.string(3)};
      }).and_then([&](const std::vector<ProjectClone> &clones)
          -> std::expected<CloneAliases, std::string> {
        CloneAliases result;
        for (const auto &file : files) {
          if (!file.component.repositoryId ||
              std::filesystem::path(file.component.path).is_absolute()) continue;
          for (const auto &clone : clones) {
            if (clone.repositoryId != *file.component.repositoryId ||
                (file.clone && clone.id == file.clone->id)) continue;
            auto path = fullProjectFilePath(file.component, clone, file.directory, file.name);
            std::error_code error;
            auto canonical = std::filesystem::weakly_canonical(path, error);
            if (error) return std::unexpected("cannot resolve clone file identity: " + error.message());
            result[static_cast<FileId>(file.id)].push_back(canonical.string());
          }
        }
        return result;
      });
}
}
