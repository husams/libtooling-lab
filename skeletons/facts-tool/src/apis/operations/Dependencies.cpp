#include "apis/operations/Compilation.h"
#include "apis/operations/Diagnostics.h"
#include "apis/operations/Details.h"
#include "commands/Dependency.h"
#include "storage/CloneContext.h"
#include "storage/SqliteDatabase.h"

namespace facts::apis::operations {
Result dependencyResult(const domain::ResolvedFile &file) {
  return storage::Database::open(file.facts.string())
      .transform_error([](auto error) { return storageError(error.message()); })
      .transform([&](storage::Database database) {
        auto result = resultBase("dependencies", file);
        auto edges = nlohmann::json::array();
        for (const auto &row : database.rows(
                 "WITH RECURSIVE reachable(id) AS (VALUES(?1) UNION "
                 "SELECT dst_file_id FROM include_dependency JOIN reachable "
                 "ON src_file_id=reachable.id) "
                 "SELECT src_file_id,dst_file_id,source.path,destination.path "
                 "FROM include_dependency "
                 "LEFT JOIN facts_project_provenance AS source "
                 "ON source.file_id=src_file_id "
                 "LEFT JOIN facts_project_provenance AS destination "
                 "ON destination.file_id=dst_file_id "
                 "WHERE src_file_id IN (SELECT id FROM reachable) "
                 "ORDER BY src_file_id,dst_file_id", file.fileId))
          edges.push_back({{"source_file_id", row.integer(0)},
                           {"destination_file_id", row.integer(1)},
                           {"source_path", row.text(2)},
                           {"destination_path", row.text(3)}});
        result["edge_count"] = edges.size();
        result["edges"] = std::move(edges);
        return result;
      });
}
Result dependencies(const domain::Context &context,
                    const domain::ResolvedFile &file) {
  return withFileContext(context, file, [&](const domain::Context &context) -> Result {
  ScopedCloneContext scope(file.clone);
  return prepareCompilation(context, file).and_then([&](CompilationContext compilation) {
  ScopedCompilationContext selected(compilation);
  return needsExtraction(file).and_then([&](bool refresh) {
    return commands::runDependencyResolved(dependencyOptions(context, file))
      .transform_error(operationError)
      .and_then(completed)
      .and_then([&] { return recordFacts(context, file, refresh, scope.refreshedFiles()); })
      .and_then([&] { return dependencyResult(file); });
  })
      .transform_error([&](domain::Error error) {
        return compilationFailure(context, file, std::move(error));
      });
  });
  });
}
}
