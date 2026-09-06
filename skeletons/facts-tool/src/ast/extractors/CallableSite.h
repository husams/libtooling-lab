#pragma once

#include "analysis/callgraph/CallGraphTypes.h"
#include "ast/extractors/Extraction.h"
#include "ast/extractors/ReceiverContext.h"

#include <clang/Basic/SourceLocation.h>

#include <optional>

namespace clang {
class FunctionDecl;
class SourceManager;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

ExtractionResult<std::optional<callgraph::CallFact>> extractCallableSite(
    const clang::FunctionDecl &caller, const clang::FunctionDecl &callee,
    clang::SourceLocation location, ReceiverContext receiver, bool implicit,
    bool virtualDispatch, const clang::SourceManager &sourceManager,
    FileManager &files, FactStore &store);

} // namespace facts
