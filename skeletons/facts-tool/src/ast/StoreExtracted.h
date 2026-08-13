#pragma once

#include "ast/extractors/File.h"
#include "storage/FactStore.h"
#include "storage/FileManager.h"

#include <clang/AST/ASTContext.h>
#include <llvm/Support/raw_ostream.h>

#include <expected>
#include <system_error>
#include <utility>

namespace facts {

template <typename Node, typename Result, typename Stored>
void storeExtracted(Node &node, Result symbol, clang::ASTContext &context,
                    FileManager &files, FactStore &store, Stored stored) {
  if (!symbol) {
    return;
  }

  auto persisted =
      resolveFile(context.getSourceManager(), node.getLocation(), files)
          .and_then([&store, &symbol](FileId file) {
            return store.save(file, std::move(*symbol));
          })
          .and_then(std::move(stored));
  if (!persisted) {
    llvm::errs() << "facts-tool: cannot persist extracted facts: "
                 << persisted.error().message() << '\n';
  }
}

template <typename Node, typename Result>
void storeExtracted(Node &node, Result symbol, clang::ASTContext &context,
                    FileManager &files, FactStore &store) {
  const auto complete = [](SymbolId) {
    return std::expected<void, std::error_code>{};
  };
  storeExtracted(node, std::move(symbol), context, files, store, complete);
}

} // namespace facts
