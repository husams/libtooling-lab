#include "tooling/CompilationContext.h"
#include <utility>

namespace facts {
namespace { thread_local const CompilationContext *selectedCompilation = nullptr; }
ScopedCompilationContext::ScopedCompilationContext(const CompilationContext &context)
    : previous_(std::exchange(selectedCompilation, &context)) {}
ScopedCompilationContext::~ScopedCompilationContext() {
  selectedCompilation = previous_;
}
const CompilationContext *invocationCompilation(const std::filesystem::path &project) {
  return selectedCompilation && selectedCompilation->project == project &&
                 !selectedCompilation->commands.empty()
             ? selectedCompilation : nullptr;
}
}
