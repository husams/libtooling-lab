#pragma once

#include "commands/match/MatchTypes.h"

#include <expected>
#include <string>

namespace clang {
class ASTContext;
}

namespace facts {
class FactStore;
class FileManager;
} // namespace facts

namespace facts::commands::match {
std::expected<void, std::string> persistRelation(const RelationMatch &match,
                                                 clang::ASTContext &context,
                                                 FileManager &files,
                                                 FactStore &store);
}
