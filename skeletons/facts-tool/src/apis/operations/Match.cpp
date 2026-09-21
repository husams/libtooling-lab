#include "apis/operations/Compilation.h"
#include "apis/operations/Diagnostics.h"
#include "apis/operations/Details.h"
#include "commands/DatabasePaths.h"
#include "commands/Match.h"
#include "commands/match/MatchResolved.h"
#include "storage/CloneContext.h"

namespace facts::apis::operations {
Result match(const domain::Context &context, const domain::ResolvedFile &file,
             const MatchRequest &request) {
  return withDiagnostics([&]() -> Result {
  if (request.query.empty())
    return std::unexpected(domain::Error{400, "invalid_request", "query is required"});
  ScopedCloneContext scope(file.clone);
  return prepareCompilation(context, file).and_then([&](CompilationContext compilation) {
  ScopedCompilationContext selected(compilation);
  return needsExtraction(file).and_then([&](bool refresh) {
    return commands::validateDatabasePaths(file.facts.string(),
                                         context.configuration.database.string())
      .transform_error(operationError)
      .and_then([&] {
        return commands::runMatchResolved(matchOptions(context, file, request), false,
                                           commands::match::BindingPolicy::Any)
            .transform_error(operationError);
      }).and_then([&](commands::match::MatchOutput output) {
        return recordFacts(context, file, refresh, scope.refreshedFiles()).transform([&] {
          auto result = resultBase("match", file);
          auto document = jsonValue(llvm::json::Value(std::move(output.document)));
          result["matches"] = std::move(document["matches"]);
          result["match_count"] = result["matches"].size();
          return result;
        });
      });
  }).transform_error([&](domain::Error error) {
        return compilationFailure(context, file, std::move(error));
      });
  });
  });
}
}
