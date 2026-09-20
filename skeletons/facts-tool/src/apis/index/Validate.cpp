#include "apis/index/Internal.h"
#include <array>

namespace facts::apis::index {
Result<void> validate(const Query &query) {
  if (query.qualifiedName.empty() || query.qualifiedName.size() > 4096 ||
      query.qualifiedName.find('\0') != std::string::npos)
    return std::unexpected("qualified_name must contain 1 to 4096 characters");
  if (query.limit == 0 || query.limit > 500)
    return std::unexpected("limit must be between 1 and 500");
  for (const auto *value : {&query.kind, &query.usr, &query.repository, &query.component})
    if (*value && ((*value)->empty() || (*value)->size() > 4096 ||
                   (*value)->find('\0') != std::string::npos))
      return std::unexpected("symbol filters must contain 1 to 4096 characters");
  return {};
}
}
