#include "apis/v2/symbols/Children.h"
#include "apis/domain/Candidates.h"
#include <set>

namespace facts::apis::v2::symbols {
domain::Result<SourceCatalog> sources(const domain::Context &context,
                                      const index::Symbol &symbol) {
  return catalog::open(context.configuration.database.string(), false)
      .and_then([](catalog::Database database) {
        return catalog::query(database, "SELECT file_id FROM api_index_invalidated_file",
            [](const storage::Row &row) { return row.integer(0); })
            .and_then([&](auto invalidated) {
              return catalog::files(database).transform([&](auto files) {
                return std::pair{std::move(files), std::move(invalidated)};
              });
            });
      })
      .transform_error(indexError)
      .and_then([&](auto values) -> domain::Result<SourceCatalog> {
        auto &[files, invalidated] = values;
        SourceCatalog catalog;
        catalog.invalidated.insert(invalidated.begin(), invalidated.end());
        std::set<std::filesystem::path> paths;
        for (auto &file : files) {
          const bool owned = symbol.repositoryScope
              ? file.component.repositoryId == symbol.scopeId
              : !file.component.repositoryId && file.component.id == symbol.scopeId;
          if (owned && !catalog.invalidated.contains(file.id)) {
            auto source = catalog::filePath(file).transform_error(indexError);
            if (!source) return std::unexpected(source.error());
            auto destination = domain::detail::factsPath(context, {file, symbol.repository, true}, *source);
            if (!destination) return std::unexpected(destination.error());
            paths.insert(destination->lexically_normal());
          }
          catalog.files.emplace(file.id, std::move(file));
        }
        catalog.sources.assign(paths.begin(), paths.end());
        return catalog;
      });
}
domain::Result<Json> children(const domain::Context &context, const StoredSymbol &symbol,
                              std::string_view child, const QueryParameters &parameters) {
  const std::set<std::string> allowed = child == "relations"
      ? std::set<std::string>{"limit", "cursor", "direction", "kind"}
      : std::set<std::string>{"limit", "cursor"};
  for (const auto &[key, value] : parameters)
    if (!allowed.contains(key) || value.empty() || value.size() > 1024 || value.find('\0') != std::string::npos)
      return std::unexpected(domain::Error{422, "invalid_query", "Unknown, empty or oversized query parameter"});
  if (parameters.contains("direction") && parameters.at("direction") != "outgoing" &&
      parameters.at("direction") != "incoming" && parameters.at("direction") != "both")
    return std::unexpected(domain::Error{422, "invalid_query", "direction must be outgoing, incoming or both"});
  return sources(context, symbol.symbol).and_then([&](const SourceCatalog &catalog) {
    return childPage(parameters, symbol, child, catalog).and_then([&](ChildPage page) -> domain::Result<Json> {
      for (const auto &path : catalog.sources) {
        std::error_code error;
        if (!std::filesystem::exists(path, error) && !error) continue;
        auto collected = facts::catalog::Database::open(path.string(), facts::catalog::Database::readOnly)
            .transform_error([](auto error) { return domain::Error{503, "facts_unavailable", error.message()}; })
            .and_then([&](facts::catalog::Database database) {
              return database.read().transform_error([](auto error) {
                return domain::Error{503, "facts_unavailable", error.message()};
              }).and_then([&](storage::Transaction transaction) {
                return collect(database, catalog, symbol, child, parameters, page);
              });
            });
        if (!collected) return std::unexpected(collected.error());
      }
      if (page.fingerprint != fingerprint(catalog, symbol.generation))
        return std::unexpected(domain::Error{409, "invalid_cursor", "Facts changed during the query; retry without a cursor"});
      return facts::catalog::open(context.configuration.database.string(), false)
          .and_then([](facts::catalog::Database database) {
            return facts::catalog::query(database,
                "SELECT generation FROM global_symbol_index_state WHERE id=1",
                [](const storage::Row &row) { return row.integer(0); });
          }).transform_error(indexError)
          .and_then([&](const auto &generation) -> domain::Result<Json> {
            if (generation.empty() || generation.front() != symbol.generation)
              return std::unexpected(domain::Error{409, "invalid_cursor", "Index changed during the query; retry without a cursor"});
            return finish(std::move(page), symbol.generation);
          });
    });
  });
}
}
