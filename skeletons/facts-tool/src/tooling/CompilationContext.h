#pragma once
#include <clang/Tooling/CompilationDatabase.h>
#include <filesystem>
#include <vector>

namespace facts {
struct CompilationContext {
  std::filesystem::path project;
  std::vector<clang::tooling::CompileCommand> commands;
};
class ScopedCompilationContext {
public:
  explicit ScopedCompilationContext(const CompilationContext &context);
  ~ScopedCompilationContext();
  ScopedCompilationContext(const ScopedCompilationContext &) = delete;
  ScopedCompilationContext &operator=(const ScopedCompilationContext &) = delete;
private:
  const CompilationContext *previous_;
};
const CompilationContext *invocationCompilation(const std::filesystem::path &project);
}
