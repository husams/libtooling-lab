#pragma once

#include "analysis/variableflow/Model.h"
#include "tooling/astcache/Options.h"

#include <clang/AST/Decl.h>
#include <clang/AST/Stmt.h>
#include <clang/Analysis/CFG.h>
#include <clang/Frontend/ASTUnit.h>
#include <clang/Tooling/CompilationDatabase.h>

#include <expected>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace clang {
class ASTContext;
class ASTUnit;
class CallGraph;
} // namespace clang

namespace facts::variableflow::detail {

struct Function {
  const clang::FunctionDecl *decl = nullptr;
  clang::ASTContext *context = nullptr;
  std::string usr;
  std::string tu;
};

struct Parsed {
  std::vector<std::unique_ptr<clang::ASTUnit>> units;
  std::vector<std::unique_ptr<Function>> functions;
  std::unordered_map<std::string, std::vector<const Function *>> byUsr;
  std::unordered_map<std::string, std::vector<const Function *>> byName;
  std::unordered_map<const clang::Expr *, const clang::FunctionDecl *>
      callTargets;
};

std::expected<Parsed, std::string> parse(clang::tooling::CompilationDatabase &,
                                        const std::vector<std::string> &,
                                        const astcache::Options &);

std::string usrFor(const clang::NamedDecl &, std::string_view tu);
std::string functionName(const clang::FunctionDecl &);
Location locationOf(const clang::SourceManager &, clang::SourceLocation);

std::expected<const Function *, std::string> selectFunction(const Parsed &,
                                                            const Request &);

std::expected<const clang::VarDecl *, std::string>
selectVariable(const Function &, const Request &);

Graph runFlow(const Parsed &, const Function &, const clang::VarDecl *,
              const Request &);

} // namespace facts::variableflow::detail
