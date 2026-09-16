#pragma once

#include <expected>
#include <string>
#include <yaml-cpp/yaml.h>

namespace facts::config::detail {

inline std::expected<bool, std::string> astCacheEnabled(const YAML::Node &node) {
  if (!node || !node.IsScalar() || node.Tag() != "?" ||
      (node.Scalar() != "true" && node.Scalar() != "false"))
    return std::unexpected("ast_cache must be a boolean (true or false)");
  return node.Scalar() == "true";
}

} // namespace facts::config::detail
