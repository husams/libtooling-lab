#include "apis/operations/Compilation.h"
#include "apis/operations/Diagnostics.h"
#include "apis/operations/Details.h"
#include "commands/Extract.h"
#include "storage/CloneContext.h"
#include "storage/SqliteDatabase.h"

namespace facts::apis::operations {
Result extractionResult(const domain::ResolvedFile &file) {
  return storage::Database::open(file.facts.string())
      .transform_error([](auto error) { return storageError(error.message()); })
      .transform([&](storage::Database database) {
        auto result = resultBase("extract", file);
        for (const auto &row : database.rows("SELECT COUNT(*) FROM symbol"))
          result["symbol_count"] = row.integer(0);
        return result;
      });
}
Result extract(const domain::Context &context, const domain::ResolvedFile &file,
               const ExtractRequest &request) {
  return withDiagnostics([&]() -> Result {
  ScopedCloneContext scope(file.clone);
  return prepareCompilation(context, file).and_then([&](CompilationContext compilation) {
  ScopedCompilationContext selected(compilation);
  return needsExtraction(file).and_then([&](bool refresh) {
        auto options = extractOptions(context, file, request);
        options.force = options.force || refresh;
        return commands::runExtractResolved(options, false)
            .transform_error(operationError);
      })
      .and_then(completed)
      .and_then([&] { return recordFacts(context, file); })
      .and_then([&] { return extractionResult(file); });
  });
  });
}
}
