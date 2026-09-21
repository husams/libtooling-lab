#include "apis/v2/JobServices.h"
#include "apis/operations/Compilation.h"
#include "apis/operations/Diagnostics.h"
#include "commands/Import.h"
#include "storage/CloneContext.h"

namespace facts::apis::v2::jobs {
Result<void> prepareFile(const domain::Context &context, const domain::ResolvedFile &file) {
  return operations::withDiagnostics([&]() -> Result<Json> {
  ScopedCloneContext clone(file.clone);
  return operations::prepareCompilation(context, file)
      .transform_error([&](domain::Error error) {
        return operations::compilationFailure(context, file, std::move(error));
      })
      .and_then([&](CompilationContext compilation) -> Result<void> {
        ScopedCompilationContext selected(compilation);
        return commands::refreshImportRegistry(context.configuration, {file.path.string()})
            .transform_error(failed)
            .transform_error([&](domain::Error error) {
              return operations::compilationFailure(context, file, std::move(error));
            });
      }).transform([] { return Json::object(); });
  }).transform([](const Json &) {});
}
}
