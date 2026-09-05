#include "commands/match/RelationKinds.h"

#include <array>

namespace facts::commands::match {
namespace {
constexpr std::array kinds{
    std::pair{"Calls", RelationKind::Calls},
    std::pair{"Inherits", RelationKind::Inherits},
    std::pair{"Contains", RelationKind::Contains},
    std::pair{"Overrides", RelationKind::Overrides},
    std::pair{"Uses", RelationKind::Uses},
    std::pair{"FieldOf", RelationKind::FieldOf},
    std::pair{"MethodOf", RelationKind::MethodOf},
};
}

std::expected<RelationKind, std::string>
parseRelationKind(std::string_view name) {
  for (const auto &[label, kind] : kinds)
    if (name == label)
      return kind;
  return std::unexpected("unsupported relation kind '" + std::string{name} +
                         "'");
}

std::string_view relationName(RelationKind kind) {
  for (const auto &[label, candidate] : kinds)
    if (kind == candidate)
      return label;
  return "unsupported";
}

bool siteBacked(RelationKind kind) {
  return kind == RelationKind::Calls || kind == RelationKind::Overrides ||
         kind == RelationKind::Uses;
}

} // namespace facts::commands::match
