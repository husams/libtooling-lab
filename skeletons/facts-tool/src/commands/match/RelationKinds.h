#pragma once

#include "model/Relation.h"

#include <expected>
#include <string>
#include <string_view>

namespace facts::commands::match {
std::expected<RelationKind, std::string>
parseRelationKind(std::string_view name);
std::string_view relationName(RelationKind kind);
bool siteBacked(RelationKind kind);
} // namespace facts::commands::match
