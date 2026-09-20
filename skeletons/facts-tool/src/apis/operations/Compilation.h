#pragma once
#include "apis/domain/Selection.h"
#include "tooling/CompilationContext.h"

namespace facts::apis::operations {
domain::Result<CompilationContext> prepareCompilation(
    const domain::Context &context, const domain::ResolvedFile &file);
}
