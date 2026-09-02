#include "commands/match/NearestCaller.h"

#include "ast/extractors/Reference.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/ASTTypeTraits.h>
#include <clang/AST/ExprCXX.h>
#include <clang/AST/ParentMapContext.h>

namespace facts::commands::match {

std::expected<const clang::FunctionDecl *, std::string>
nearestCaller(const clang::CallExpr &call, clang::ASTContext &context) {
  clang::DynTypedNode node = clang::DynTypedNode::create(call);
  for (;;) {
    auto parents = context.getParents(node);
    if (parents.size() != 1)
      return std::unexpected(parents.empty()
                                 ? "call has no enclosing callable"
                                 : "call has ambiguous enclosing callable");
    node = parents[0];
    if (const auto *function = node.get<clang::FunctionDecl>())
      return &referenceOwner(*function);
    if (node.get<clang::BlockDecl>() || node.get<clang::CXXDefaultArgExpr>() ||
        node.get<clang::CXXDefaultInitExpr>() || node.get<clang::FieldDecl>())
      return std::unexpected("call is not owned by a function body");
  }
}

} // namespace facts::commands::match
