#pragma once

#include "ast/Indexing.h"
#include "ast/extractors/Extraction.h"
#include "ast/extractors/File.h"
#include "ast/extractors/Type.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

#include <clang/AST/ASTContext.h>

#include <expected>
#include <string>
#include <system_error>
#include <utility>

namespace facts {

inline bool isFilteredExtraction(ExtractionError error) {
  return error == ExtractionError::OutsideMainFile ||
         error == ExtractionError::SystemHeader;
}

inline std::string_view extractionErrorName(ExtractionError error) {
  switch (error) {
  case ExtractionError::InvalidSourceLocation:
    return "invalid source location";
  case ExtractionError::InvalidSourceRange:
    return "invalid source range";
  case ExtractionError::OutsideMainFile:
    return "outside indexed files";
  case ExtractionError::SystemHeader:
    return "system header";
  case ExtractionError::InvalidPresumedLocation:
    return "invalid presumed location";
  case ExtractionError::InvalidUsr:
    return "invalid USR";
  case ExtractionError::InvalidType:
    return "invalid type";
  }
  return "unknown extraction error";
}

inline bool isFilteredExtraction(const DetailedExtractionError &error) {
  const auto *extraction = std::get_if<ExtractionError>(&error);
  return extraction != nullptr && isFilteredExtraction(*extraction);
}

inline std::string extractionErrorName(const DetailedExtractionError &error) {
  if (const auto *extraction = std::get_if<ExtractionError>(&error)) {
    return std::string{extractionErrorName(*extraction)};
  }
  const auto &type = std::get<TypeResolutionError>(error);
  return "type target='" + type.target + "' usr='" + type.usr +
         "': " + type.detail;
}

template <typename Node, typename Error>
IndexingResult extractionFailure(const Node &node, const Error &error) {
  if (isFilteredExtraction(error)) {
    return {};
  }
  return std::unexpected(IndexingError{
      "cannot extract symbol '" + node.getQualifiedNameAsString() +
      "': " + std::string{extractionErrorName(error)}});
}

inline IndexingResult
postSaveResult(std::expected<void, std::error_code> result,
               std::string_view context) {
  return withContext(std::move(result), context);
}

inline IndexingResult postSaveResult(IndexingResult result,
                                     std::string_view context) {
  return withContext(std::move(result), context);
}

template <typename Node, typename Result, typename Stored>
IndexingResult storeExtracted(Node &node, Result symbol,
                              clang::ASTContext &context, FileManager &files,
                              FactStore &store, Stored stored) {
  const auto *nodeKind =
      static_cast<const clang::Decl &>(node).getDeclKindName();
  if (!symbol) {
    cli::logVerbose(store.verbosity(), 3,
                    "facts-tool: trace: node extraction kind='{}' name='{}' "
                    "result={} reason='{}'",
                    nodeKind, node.getQualifiedNameAsString(),
                    isFilteredExtraction(symbol.error()) ? "filtered"
                                                         : "failure",
                    extractionErrorName(symbol.error()));
    return extractionFailure(node, symbol.error());
  }

  const auto name = node.getQualifiedNameAsString();
  cli::logVerbose(store.verbosity(), 3,
                  "facts-tool: trace: node extraction kind='{}' name='{}' "
                  "result=success",
                  nodeKind, name);
  return resolveFile(context.getSourceManager(), node.getLocation(), files)
      .transform([&](FileId file) {
        cli::logVerbose(store.verbosity(), 3,
                        "facts-tool: trace: node file resolution kind='{}' "
                        "name='{}' result=found file_id={}",
                        nodeKind, name, file);
        return file;
      })
      .transform_error([&](std::error_code error) {
        cli::logVerbose(store.verbosity(), 3,
                        "facts-tool: trace: node file resolution kind='{}' "
                        "name='{}' result=failure error='{}'",
                        nodeKind, name, error.message());
        return IndexingError{"cannot resolve source file for '" + name +
                                 "': " + error.message(),
                             error == std::errc::no_such_file_or_directory
                                 ? "missing-file-identity"
                                 : std::string{}};
      })
      .and_then([&store, &symbol, &name](FileId file) {
        return withContext(store.save(file, std::move(*symbol)),
                           "cannot persist symbol '" + name + "'");
      })
      .and_then([stored = std::move(stored), name](SymbolId id) mutable {
        return postSaveResult(std::move(stored)(id),
                              "cannot persist post-save facts for '" + name +
                                  "'");
      });
}

template <typename Node, typename Result>
IndexingResult storeExtracted(Node &node, Result symbol,
                              clang::ASTContext &context, FileManager &files,
                              FactStore &store) {
  const auto complete = [](SymbolId) {
    return std::expected<void, std::error_code>{};
  };
  return storeExtracted(node, std::move(symbol), context, files, store,
                        complete);
}

} // namespace facts
