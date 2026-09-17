#pragma once

#include "ast/extractors/Extraction.h"
#include "model/PointerCallSite.h"

#include <optional>

namespace clang {
class ASTContext;
class CallExpr;
class FunctionDecl;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

bool isPointerCall(const clang::CallExpr &call);

ExtractionResult<std::optional<PointerCallSite>>
extractPointerCallSite(const clang::FunctionDecl &caller,
                       const clang::CallExpr &call, clang::ASTContext &context,
                       FileManager &files, FactStore &store);

} // namespace facts
