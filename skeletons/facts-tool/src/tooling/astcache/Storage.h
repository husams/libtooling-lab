#pragma once

#include "ast/visitors/IncludeVisitor.h"
#include "tooling/astcache/Metadata.h"

#include <expected>
#include <memory>
#include <string>

namespace clang {
class ASTUnit;
}

namespace facts::astcache::detail {

std::unique_ptr<clang::ASTUnit> loadAST(const Entry &entry,
                                      IncludeGraphFacts *includes = nullptr);
// Validate the prepared AST and return its persisted dependencies without
// deserializing the artifact during import.
std::optional<IncludeGraphFacts> preparedIncludes(const Entry &entry);
std::expected<void, std::string> storeAST(const Entry &entry,
                                        clang::ASTUnit &unit,
                                        const Snapshot &snapshot);

} // namespace facts::astcache::detail
