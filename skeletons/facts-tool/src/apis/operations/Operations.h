#pragma once
#include "apis/domain/Selection.h"
#include <nlohmann/json.hpp>

namespace facts::apis::operations {
struct ExtractRequest {
  bool force = false;
};
struct MatchRequest {
  std::string query;
  std::optional<std::string> traversal;
  std::optional<std::string> relationKind;
  bool captureSource = false;
};
using Result = domain::Result<nlohmann::json>;
Result extract(const domain::Context &context, const domain::ResolvedFile &file,
               const ExtractRequest &request = {});
Result match(const domain::Context &context, const domain::ResolvedFile &file,
             const MatchRequest &request);
Result dependencies(const domain::Context &context,
                    const domain::ResolvedFile &file);
}
