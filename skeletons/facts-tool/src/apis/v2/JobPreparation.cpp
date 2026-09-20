#include "apis/v2/JobServices.h"
#include "apis/operations/Compilation.h"
#include "commands/Import.h"
#include "storage/CloneContext.h"

namespace facts::apis::v2::jobs {
Result<void> prepareFile(const domain::Context &context, const domain::ResolvedFile &file) {
  ScopedCloneContext clone(file.clone);
  return operations::prepareCompilation(context, file)
      .and_then([&](CompilationContext compilation) -> Result<void> {
        ScopedCompilationContext selected(compilation);
        return commands::refreshImportRegistry(context.configuration, {file.path.string()})
            .transform_error(failed);
      });
}
}
