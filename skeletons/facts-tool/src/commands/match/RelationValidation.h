#pragma once

#include "model/Relation.h"

#include <expected>
#include <string>

namespace clang {
class NamedDecl;
}

namespace facts::commands::match {
std::expected<void, std::string>
validateEndpoints(RelationKind kind, const clang::NamedDecl &source,
                  const clang::NamedDecl &target);
}
