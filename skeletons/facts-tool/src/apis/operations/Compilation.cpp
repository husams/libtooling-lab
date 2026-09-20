#include "apis/operations/Compilation.h"
#include "apis/operations/compilation/Candidates.h"
#include "tooling/CompilationCommandCodec.h"

namespace facts::apis::operations {
domain::Result<CompilationContext> prepareCompilation(
    const domain::Context &context, const domain::ResolvedFile &file) {
  const auto project = normalizeCompilationPath(context.configuration.database);
  return compilation::hasCommand(context, file)
      .and_then([&](bool direct) -> domain::Result<CompilationContext> {
        if (direct) return CompilationContext{project, {}};
        return compilation::candidates(context).and_then([&](auto candidates) {
          return compilation::knownIncluders(file).and_then([&](const auto &known) {
            return compilation::selectContext(context, file,
                compilation::groupCandidates(std::move(candidates), file.path, known));
          });
        }).transform([&](auto command) {
          return CompilationContext{project, {std::move(command)}};
        });
      });
}
}
