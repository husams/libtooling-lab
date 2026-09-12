#pragma once

#include <clang/AST/ASTTypeTraits.h>
#include <llvm/Support/JSON.h>

namespace clang {
class ASTContext;
}

namespace facts::commands::match {

std::string sourcePath(const clang::SourceManager &source,
                       clang::SourceLocation location);
llvm::json::Object describeBinding(const clang::DynTypedNode &node,
                                   const clang::ASTContext &context);

} // namespace facts::commands::match
