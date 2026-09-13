#include "analysis/variableflow/Internal.h"

#include "analysis/variableflow/Signature.h"

#include <clang/AST/Decl.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/Linkage.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Index/USRGeneration.h>
#include <llvm/ADT/SmallString.h>

#include <algorithm>
#include <string_view>
#include <utility>

namespace facts::variableflow::detail {

std::string usrFor(const clang::NamedDecl &decl, std::string_view tu) {
  llvm::SmallString<256> buffer;
  if (clang::index::generateUSRForDecl(&decl, buffer)) {
    const auto location = decl.getLocation();
    const auto offset = location.isValid() ? location.getRawEncoding() : 0;
    return std::string{tu} + "#raw:" + std::to_string(offset);
  }
  auto result = buffer.str().str();
  if (decl.getFormalLinkage() == clang::Linkage::Internal ||
      decl.getFormalLinkage() == clang::Linkage::UniqueExternal)
    result += "@tu:" + std::string{tu};
  return result;
}

std::string functionName(const clang::FunctionDecl &decl) {
  return decl.getQualifiedNameAsString();
}

Location locationOf(const clang::SourceManager &manager,
                    clang::SourceLocation location) {
  Location result;
  if (location.isInvalid())
    return result;
  const auto spelling = manager.getExpansionLoc(location);
  const auto presumed = manager.getPresumedLoc(spelling);
  if (presumed.isValid()) {
    result.file = presumed.getFilename();
    result.line = presumed.getLine();
    result.column = presumed.getColumn();
  }
  if (spelling.isValid())
    result.offset = manager.getFileOffset(spelling);
  return result;
}

std::expected<const Function *, std::string>
selectFunction(const Parsed &parsed, const Request &request) {
  std::vector<const Function *> candidates;
  const auto byUsr = parsed.byUsr.find(request.function);
  if (byUsr != parsed.byUsr.end())
    candidates = byUsr->second;
  if (candidates.empty() && request.function.find('(') != std::string::npos)
    for (const auto &function : parsed.functions)
      if (matchesFunctionSignature(*function->decl, request.function))
        candidates.push_back(function.get());
  if (candidates.empty()) {
    const auto byName = parsed.byName.find(request.function);
    if (byName != parsed.byName.end())
      candidates = byName->second;
  }
  if (candidates.empty())
    return std::unexpected("function not found: " + request.function);
  std::ranges::sort(candidates, [](const auto *left, const auto *right) {
    return std::pair{left->usr, left->tu} < std::pair{right->usr, right->tu};
  });
  candidates.erase(
      std::ranges::unique(candidates, {},
                          [](const auto *function) { return function->usr; })
          .begin(),
      candidates.end());
  if (candidates.size() != 1) {
    std::string error = "ambiguous function selector: " + request.function;
    for (const auto *candidate : candidates)
      error += "\n  " + describeFunction(*candidate->decl, candidate->usr);
    return std::unexpected(std::move(error));
  }
  return candidates.front();
}

std::expected<const clang::VarDecl *, std::string>
selectVariable(const Function &function, const Request &request) {
  std::vector<const clang::VarDecl *> candidates;
  for (const auto *decl : function.decl->parameters()) {
    if (request.variable == decl->getNameAsString() ||
        request.variable == usrFor(*decl, function.tu))
      candidates.push_back(decl);
  }

  class Variables final : public clang::RecursiveASTVisitor<Variables> {
  public:
    Variables(std::vector<const clang::VarDecl *> &output,
              const Request &request, std::string_view tu)
        : output_(output), request_(request), tu_(tu) {}

    bool VisitVarDecl(const clang::VarDecl *decl) {
      if (request_.variable == decl->getNameAsString() ||
          request_.variable == usrFor(*decl, tu_))
        output_.push_back(decl);
      return true;
    }

    bool TraverseLambdaExpr(clang::LambdaExpr *lambda) {
      for (auto *capture : lambda->capture_inits())
        if (!TraverseStmt(capture))
          return false;
      return true;
    }

    bool TraverseFunctionDecl(clang::FunctionDecl *) { return true; }

  private:
    std::vector<const clang::VarDecl *> &output_;
    const Request &request_;
    std::string_view tu_;
  } visitor(candidates, request, function.tu);

  visitor.TraverseStmt(function.decl->getBody());
  if (candidates.empty())
    return std::unexpected("variable not found: " + request.variable);
  if (request.line) {
    std::erase_if(candidates, [&](const auto *decl) {
      const auto line = decl->getASTContext()
                            .getSourceManager()
                            .getPresumedLoc(decl->getLocation())
                            .getLine();
      return line != *request.line;
    });
  }
  if (candidates.empty())
    return std::unexpected("variable not found at line " +
                           std::to_string(*request.line) + ": " +
                           request.variable);
  std::ranges::sort(candidates, [&](const auto *left, const auto *right) {
    return left->getLocation().getRawEncoding() <
           right->getLocation().getRawEncoding();
  });
  candidates.erase(std::ranges::unique(candidates).begin(), candidates.end());
  if (candidates.size() != 1)
    return std::unexpected("ambiguous variable selector: " + request.variable +
                           "; provide --line or a USR");
  return candidates.front();
}

} // namespace facts::variableflow::detail
