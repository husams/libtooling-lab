#pragma once
#include "apis/v2/symbols/Internal.h"
#include "storage/catalog/Records.h"
#include <map>
#include <set>

namespace facts::apis::v2::symbols {
struct SourceCatalog {
  std::map<std::int64_t, catalog::File> files;
  std::vector<std::filesystem::path> sources;
  std::set<std::int64_t> invalidated;
};
struct ChildPage {
  unsigned limit = 50;
  std::string after;
  std::string identity;
  std::string fingerprint;
  std::map<std::string, Json> items;
};
std::string digest(const std::string &);
std::string fingerprint(const SourceCatalog &, std::int64_t generation);
domain::Result<SourceCatalog> sources(const domain::Context &, const index::Symbol &);
domain::Result<ChildPage> childPage(const QueryParameters &, const StoredSymbol &,
                                  std::string_view child, const SourceCatalog &);
Json finish(ChildPage, std::int64_t generation);
domain::Result<void> collect(catalog::Database &, const SourceCatalog &,
                            const StoredSymbol &, std::string_view child,
                            const QueryParameters &, ChildPage &);
}
