#pragma once
#include "apis/watch/catalog/Catalog.h"

namespace facts::apis::watch {
inline const Clone *owner(const std::filesystem::path &path, const Catalog &catalog) {
  const Clone *result = nullptr;
  for (const auto &clone : catalog.clones) {
    const auto relative = path.lexically_relative(clone.path);
    if (!relative.empty() && !relative.is_absolute() && *relative.begin() != ".." &&
        (!result || clone.path.native().size() > result->path.native().size()))
      result = &clone;
  }
  return result;
}
}
