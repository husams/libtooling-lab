#pragma once

#include "ast/visitors/IncludeVisitor.h"
#include "model/AstCache.h"

#include <expected>
#include <string>

namespace clang {
class SourceManager;
class Preprocessor;
}

namespace facts::astcache::detail {
struct Entry;

std::expected<Snapshot, std::string>
captureSnapshot(const Entry &entry, clang::SourceManager &manager,
                clang::Preprocessor &preprocessor,
                const IncludeGraphFacts &includes);

bool currentSnapshot(const Snapshot &snapshot);
IncludeGraphFacts includesFromSnapshot(const Snapshot &snapshot);
} // namespace facts::astcache::detail
