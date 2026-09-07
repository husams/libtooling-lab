#pragma once

#include "model/ExpressionEvidence.h"

#include <expected>
#include <string>
#include <unordered_map>
#include <vector>

namespace clang {
class ASTContext;
class Expr;
class NamedDecl;
}

namespace facts {
class FactStore;
class FileManager;

namespace commands::match {

using SourceFingerprintCache = std::unordered_map<unsigned, std::string>;

std::expected<void, std::string>
captureExpression(const clang::Expr &expression, clang::ASTContext &context,
                  FileManager &files, FactStore &store,
                  SourceFingerprintCache &fingerprints);

std::expected<void, std::string>
captureSourceRegion(const clang::NamedDecl &symbol, SymbolId id,
                    clang::ASTContext &context, FileManager &files,
                    FactStore &store, SourceFingerprintCache &fingerprints);

} // namespace commands::match
} // namespace facts
