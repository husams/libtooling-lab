#pragma once

#include "commands/match/MatchTypes.h"

#include <clang/ASTMatchers/ASTMatchers.h>

#include <expected>
#include <optional>
#include <string>

namespace facts::commands::match {
std::expected<Contract, std::string>
classify(const clang::ast_matchers::BoundNodes &nodes,
         const std::optional<std::string> &relationKind);
}
