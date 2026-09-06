#include "commands/match/MatchPublication.h"

#include "storage/FactStore.h"
#include "storage/MatchedSymbolIndex.h"
#include "storage/catalog/Database.h"

#include <filesystem>

namespace facts::commands::match {
namespace {
using Result = std::expected<int, std::string>;

bool combined(const cli::MatchOptions &options) {
  return std::filesystem::absolute(options.facts).lexically_normal() ==
         std::filesystem::absolute(options.configuration).lexically_normal();
}

catalog::Result<void> publish(const std::string &path,
                              std::span<const MatchedSymbol> symbols) {
  return catalog::open(path, true).and_then([&](catalog::Database database) {
    return database.write()
        .transform_error([](auto error) { return error.message(); })
        .and_then([&](storage::Transaction transaction) {
          return storage::upsertMatchedSymbols(database, symbols)
              .transform_error([](auto error) { return error.message(); })
              .and_then([&] {
                return transaction.commit().transform_error(
                    [](auto error) { return error.message(); });
              });
        });
  });
}
} // namespace

Result finishMatch(FactStore &store, const cli::MatchOptions &options,
                   int status, std::optional<std::string> error,
                   std::span<const MatchedSymbol> symbols,
                   std::span<const FileId> selected,
                   const FactPairProvenanceSnapshot *pairing) {
  if (status != 0 || error) {
    auto finished = store.rollback();
    if (!finished)
      return std::unexpected("cannot finish facts transaction: " +
                             finished.error().message());
    return error ? Result{std::unexpected(*error)}
                 : Result{std::unexpected("translation unit matching failed")};
  }
  if (combined(options) && !symbols.empty()) {
    if (auto indexed = store.upsertMatchedSymbols(symbols); !indexed) {
      (void)store.rollback();
      return std::unexpected("match-index-write-failed "
                             "{facts_committed:false,index_committed:false}: " +
                             indexed.error().message());
    }
  }
  if (pairing) {
    if (auto registered = registerFactPairProvenance(store, *pairing, selected);
        !registered) {
      (void)store.rollback();
      return std::unexpected(registered.error());
    }
  }
  auto finished = store.end();
  if (!finished)
    return std::unexpected("cannot finish facts transaction: " +
                           finished.error().message());
  if (!combined(options) && !symbols.empty()) {
    if (auto indexed = publish(options.configuration, symbols); !indexed)
      return std::unexpected("match-index-write-failed "
                             "{facts_committed:true,index_committed:false}: " +
                             indexed.error());
  }
  return 0;
}

} // namespace facts::commands::match
