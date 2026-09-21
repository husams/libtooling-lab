#include "commands/match/MatchFrontend.h"
#include "tooling/astcache/Cache.h"

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/Diagnostic.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/CompilationDatabase.h>
#include <cassert>
#include <filesystem>
#include <fstream>

namespace {
class CheckDiagnostics final : public clang::DiagnosticConsumer {
public:
  explicit CheckDiagnostics(std::filesystem::path cwd) : cwd_(std::move(cwd)) {}
  void HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                        const clang::Diagnostic &info) override {
    clang::DiagnosticConsumer::HandleDiagnostic(level, info);
    assert(std::filesystem::current_path() == cwd_);
    ++seen;
  }
  int seen = 0;
private:
  std::filesystem::path cwd_;
};
class CheckMatch final : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
  explicit CheckMatch(std::filesystem::path cwd) : cwd_(std::move(cwd)) {}
  void run(const clang::ast_matchers::MatchFinder::MatchResult &) override {
    assert(std::filesystem::current_path() == cwd_);
    ++seen;
  }
  int seen = 0;
private:
  std::filesystem::path cwd_;
};
}

int main(int argc, char **argv) {
  assert(argc == 2);
  const auto cwd = std::filesystem::current_path();
  const auto root = std::filesystem::absolute(argv[1]);
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / "include");
  std::ofstream(root / "include/value.hpp") << "#define VALUE 7\n";
  const auto source = (root / "main.cpp").string();
  std::ofstream(source) << "#warning observe cwd during parsing\n"
                          "#include <value.hpp>\n"
                          "int cwd_probe() { return VALUE; }\n";
  clang::tooling::FixedCompilationDatabase database(root.string(), {"-Iinclude"});
  for (const bool cached : {false, true}) {
    facts::astcache::Options options;
    options.enabled = cached;
    CheckDiagnostics diagnostics(cwd);
    std::vector<std::unique_ptr<clang::ASTUnit>> units;
    assert(facts::astcache::buildASTs(database, {source}, units, options, &diagnostics) == 0);
    assert(diagnostics.seen > 0 && units.size() == 1);
    assert(std::filesystem::current_path() == cwd);
  }
  CheckMatch callback(cwd);
  clang::ast_matchers::MatchFinder finder;
  finder.addMatcher(clang::ast_matchers::functionDecl(
      clang::ast_matchers::hasName("cwd_probe")), &callback);
  facts::astcache::Options options;
  options.enabled = false;
  assert(facts::commands::match::runTranslationUnit(database, finder, source, 0, 1, options).status == 0);
  assert(callback.seen == 1);
  assert(std::filesystem::current_path() == cwd);
  std::filesystem::remove_all(root);
}
