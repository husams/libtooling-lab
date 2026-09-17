#include "ast/Indexing.h"
#include "ast/extractors/PointerCallSite.h"
#include "ast/visitors/Traversal.h"
#include "model/Relation.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

#include <clang/Tooling/Tooling.h>
#include <clang/AST/Expr.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/FileSystem.h>
#include <sqlite3.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unistd.h>

namespace {

void require(bool condition, std::string_view message) {
  if (!condition)
    throw std::runtime_error(std::string{message});
}

int scalar(sqlite3 *database, const std::string &sql) {
  sqlite3_stmt *statement = nullptr;
  const auto prepared =
      sqlite3_prepare_v2(database, sql.c_str(), -1, &statement, nullptr);
  require(prepared == SQLITE_OK, std::string{sqlite3_errmsg(database)} + ": " + sql);
  const auto stepped = sqlite3_step(statement);
  require(stepped == SQLITE_ROW, std::string{sqlite3_errmsg(database)} + ": " + sql);
  const auto value = sqlite3_column_int(statement, 0);
  sqlite3_finalize(statement);
  return value;
}

class StderrCapture {
public:
  StderrCapture() : output_(std::tmpfile()), saved_(::dup(STDERR_FILENO)) {
    require(output_ && saved_ >= 0, "cannot capture extractor diagnostics");
    llvm::errs().flush();
    require(::dup2(::fileno(output_), STDERR_FILENO) >= 0,
            "cannot redirect extractor diagnostics");
  }

  ~StderrCapture() {
    llvm::errs().flush();
    ::dup2(saved_, STDERR_FILENO);
    ::close(saved_);
    std::fclose(output_);
  }

  std::string text() {
    llvm::errs().flush();
    std::fflush(stderr);
    std::rewind(output_);
    std::string result;
    std::array<char, 4096> buffer;
    while (const auto count = std::fread(buffer.data(), 1, buffer.size(), output_))
      result.append(buffer.data(), count);
    return result;
  }

private:
  std::FILE *output_;
  int saved_;
};

void check(const std::filesystem::path &directory) {
  const std::string code = R"cpp(
#include "external.hpp"
using Callback = int (*)(double, const char *);
int first(double, const char *) { return 1; }
int second(double, const char *) { return 2; }
Callback choose();
Callback global = first;
void parameter(Callback fp) { fp(1, "a"); }
void wrappers(Callback fp) { (**fp)(1, "a"); }
void local() { auto fp = first; fp(1, "a"); }
void reassigned() { auto fp = first; fp = second; fp(1, "a"); }
void global_call() { global(1, "a"); }
struct Holder { Callback callback; };
void field(Holder &holder) { holder.callback(1, "a"); }
void array(Callback *callbacks, int index) { callbacks[index](1, "a"); }
void factory() { choose()(1, "a"); }
void selector(bool condition, Callback a, Callback b) {
  (condition ? a : b)(1, "a");
}
void dereference(Callback *callbacks) { (**callbacks)(1, "a"); }
void reference(int (&fn)(double, const char *)) { fn(1, "a"); }
void pointer_reference(Callback &fp) { fp(1, "a"); }
struct Receiver { int method(double) const; };
using Member = int (Receiver::*)(double) const;
void member(Receiver &receiver, Member fp) { (receiver.*fp)(1); }
void variadic(int (*fp)(const char *, ...)) { fp("%d", 1); }
void nothrow(int (*fp)(double) noexcept) { fp(1); }
struct Functor { int operator()(double) const { return 1; } };
void functor(Functor &object) { object(1); }
void direct() { external(); }
)cpp";
  const auto source = directory / "pointers.cpp";
  const auto header = directory / "external.hpp";
  std::ofstream(source) << code;
  std::ofstream(header) << "#pragma GCC system_header\nvoid external();\n";
  auto ast = clang::tooling::buildASTFromCodeWithArgs(
      code, {"-std=c++23", "-nostdinc", "-nostdinc++", "-I" + directory.string()},
      source.string());
  require(ast && !ast->getDiagnostics().hasErrorOccurred(), "fixture parse failed");

  const auto databasePath = directory / "facts.sqlite";
  std::string diagnostics;
  {
    StderrCapture capture;
    facts::FileManager files((directory / "registry.sqlite").string());
    require(files.addBulk(std::array{source.string(), header.string()}).has_value(),
            "cannot register source");
    facts::FactStore store(databasePath.string());
    require(store.begin().has_value(), "cannot start extraction");
    for (const auto *decl : ast->getASTContext().getTranslationUnitDecl()->decls()) {
      const auto *function = llvm::dyn_cast<clang::FunctionDecl>(decl);
      if (!function || function->getNameAsString() != "parameter")
        continue;
      const auto *body = llvm::cast<clang::CompoundStmt>(function->getBody());
      const auto *call = llvm::cast<clang::CallExpr>(*body->body_begin());
      const auto result = facts::extractPointerCallSite(
          *function, *call, ast->getASTContext(), files, store);
      require(!result && result.error() == facts::ExtractionError::RelationTarget,
              "missing persisted caller silently dropped pointer-call evidence");
    }
    facts::IndexingStatus status;
    facts::traverse(ast->getASTContext(), files, store, status);
    require(status.complete(), "extraction failed: " + capture.text());
    require(store.end(false).has_value(), "cannot commit extraction");
    diagnostics = capture.text();
  }
  require(diagnostics.find("coverage.unsupported_semantics") == std::string::npos,
          "pointer calls reported unsupported semantics: " + diagnostics);
  require(diagnostics.find("unresolved") == std::string::npos,
          "pointer calls reported unresolved semantics: " + diagnostics);

  sqlite3 *database = nullptr;
  require(sqlite3_open(databasePath.c_str(), &database) == SQLITE_OK,
          "cannot open extracted facts");
  const auto sites = [&](std::string_view caller, std::string_view condition) {
    return scalar(database,
                  "SELECT COUNT(*) FROM callgraph_pointer_call_site p "
                  "JOIN symbol s ON s.id=p.source_id WHERE s.qualified_name='" +
                      std::string{caller} + "' AND " + std::string{condition});
  };
  for (const auto caller : {"parameter", "wrappers", "local", "reassigned", "global_call",
                            "field"}) {
    require(sites(caller, "target_id IS NOT NULL AND signature='int (*)(double, const char *)'") == 1,
            std::string{caller} + " lost pointer identity or canonical signature");
  }
  for (const auto caller : {"array", "factory", "selector", "dereference"}) {
    require(sites(caller, "target_id IS NULL AND signature='int (*)(double, const char *)'") == 1,
            std::string{caller} + " fabricated an operand or lost callable type");
  }
  require(sites("reference", "signature='int (&)(double, const char *)' AND target_id IS NOT NULL") == 1,
          "function reference lost its declared signature");
  require(sites("pointer_reference", "signature='int (*&)(double, const char *)' AND target_id IS NOT NULL") == 1,
          "reference to pointer lost its declared signature");
  require(sites("member", "signature='int (Receiver::*)(double) const' AND target_id IS NOT NULL") == 1,
          "member pointer lost class or const qualifier");
  require(sites("variadic", "signature='int (*)(const char *, ...)' AND target_id IS NOT NULL") == 1,
          "variadic pointer lost its signature");
  require(sites("nothrow", "signature='int (*)(double) noexcept' AND target_id IS NOT NULL") == 1,
          "noexcept pointer lost its exception specification");
  require(sites("functor", "1") == 0 && sites("direct", "1") == 0,
          "ordinary direct calls became pointer calls");
  require(sites("factory", "expression='choose()'") == 1,
          "factory call lost callee expression");
  require(sites("field", "expression='holder.callback'") == 1,
          "field call lost callee expression");
  require(scalar(database, "SELECT COUNT(*) FROM callgraph_pointer_call_site WHERE line=0 OR col=0 OR expression='' OR signature=''") == 0,
          "pointer calls have incomplete source evidence");
  require(scalar(database, "SELECT COUNT(*) FROM callgraph_unresolved_site") == 0,
          "supported pointer calls persisted unresolved evidence");
  require(scalar(database, "SELECT COUNT(*) FROM callgraph_pointer_call_site p JOIN symbol d ON d.id=p.target_id WHERE d.is_external=1") == 0,
          "local pointer values were mislabeled external");
  require(scalar(database, "SELECT COUNT(*) FROM callgraph_pointer_call_site p WHERE p.target_id IS NOT NULL AND NOT EXISTS (SELECT 1 FROM relation_site r WHERE r.kind=24 AND r.source_id=p.source_id AND r.destination_id=p.target_id AND r.file_id=p.file_id AND r.line=p.line AND r.col=p.col)") == 0,
          "named pointer invocation lost its relation site");
  require(scalar(database, "SELECT COUNT(*) FROM relation_site r JOIN symbol s ON s.id=r.source_id JOIN symbol d ON d.id=r.destination_id WHERE r.kind=1 AND s.qualified_name='local' AND d.qualified_name='first'") == 1,
          "proven local function target lost its Calls edge");
  require(scalar(database, "SELECT COUNT(*) FROM relation_site r JOIN symbol s ON s.id=r.source_id JOIN symbol d ON d.id=r.destination_id WHERE r.kind=1 AND s.qualified_name='reassigned' AND d.qualified_name IN ('first','second')") == 0,
          "reassigned pointer guessed an exact function target");
  require(scalar(database, "SELECT COUNT(*) FROM relation_site r JOIN symbol s ON s.id=r.source_id JOIN symbol d ON d.id=r.destination_id WHERE r.kind=1 AND s.qualified_name='direct' AND d.qualified_name='external' AND d.is_external=1") == 1,
          "known external function lost its ordinary Calls edge");
  sqlite3_close(database);
}

} // namespace

int main() {
  llvm::SmallString<256> directory;
  if (const auto error =
          llvm::sys::fs::createUniqueDirectory("facts-pointer-calls", directory)) {
    std::cerr << error.message() << '\n';
    return 1;
  }
  int result = 0;
  try {
    check(directory.str().str());
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    result = 1;
  }
  std::filesystem::remove_all(directory.str().str());
  return result;
}
