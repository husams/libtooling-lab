#pragma once

#include "model/AstCache.h"

#include <expected>
#include <memory>
#include <string>

namespace clang::tooling {
class FrontendActionFactory;
}

namespace facts {
struct IncludeGraphFacts;
}

namespace facts::astcache::detail {
struct Entry;
struct RevisionObservations;

using CapturedSnapshot = std::expected<Snapshot, std::string>;

std::unique_ptr<clang::tooling::FrontendActionFactory>
createPreprocessFactory(const Entry &entry, IncludeGraphFacts &includes,
                        CapturedSnapshot &snapshot,
                        RevisionObservations &observations);

} // namespace facts::astcache::detail
