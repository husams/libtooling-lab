#include "apis/operations/compilation/Candidates.h"
#include "apis/operations/Details.h"
#include "storage/SqliteDatabase.h"
#include "tooling/CompilationCommandCodec.h"
#include "tooling/StoredCompilationReader.h"

namespace facts::apis::operations::compilation {
domain::Result<bool> hasCommand(const domain::Context &context,
                                const domain::ResolvedFile &file) {
  return storage::Database::open(context.configuration.database.string())
      .transform_error([](auto error) { return storageError(error.message()); })
      .and_then([&](storage::Database database) -> domain::Result<bool> {
        for (const auto &row : database.rows(
                 "SELECT compile_options IS NOT NULL FROM file WHERE id=?1", file.fileId))
          return row.integer(0) != 0;
        return std::unexpected(domain::Error{404, "file_not_found", "registered file not found"});
      });
}
domain::Result<std::vector<Candidate>> candidates(const domain::Context &context) {
  return openStoredDatabase(context.configuration.database)
      .and_then([](StoredDatabase database) { return readStoredCompilation(database.get()); })
      .and_then([](const StoredCompilationSnapshot &snapshot) {
        return decodeCompileCommands(snapshot).transform([&](auto commands) {
          std::vector<Candidate> result;
          for (std::size_t index = 0; index < commands.size(); ++index)
            result.push_back({snapshot.files[index].id, std::move(commands[index])});
          return result;
        });
      }).transform_error([](std::string error) {
        return domain::Error{422, "compilation_context_unavailable", std::move(error)};
      });
}
}
