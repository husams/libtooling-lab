#pragma once

#include "ast/extractors/Extraction.h"
#include "model/ReceiverCertainty.h"
#include "model/SymbolId.h"

#include <optional>

namespace clang {
class CXXRecordDecl;
class Expr;
class SourceManager;
} // namespace clang

namespace facts {
class FactStore;
class FileManager;

struct ReceiverContext {
  const clang::CXXRecordDecl *declaration = nullptr;
  std::optional<SymbolId> type;
  std::optional<ReceiverCertainty> certainty;
};

ExtractionResult<ReceiverContext>
extractReceiverContext(const clang::Expr &site,
                       const clang::SourceManager &sourceManager,
                       FileManager &files, FactStore &store);

} // namespace facts
