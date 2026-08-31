#include "analysis/callgraph/OverrideIndex.h"

#include <clang/AST/DeclCXX.h>

namespace facts::callgraph {

std::vector<const OverrideFact *>
OverrideIndex::targets(const clang::CXXMethodDecl &base,
                       const clang::CXXRecordDecl &receiver,
                       ReceiverCertainty certainty) const {
  std::vector<const OverrideFact *> matches;
  const auto *canonicalBase = base.getCanonicalDecl();
  const auto *canonicalReceiver = receiver.getCanonicalDecl();
  for (const auto &override : overrides_) {
    if (override.destination->getCanonicalDecl() != canonicalBase)
      continue;
    if (certainty == ReceiverCertainty::Exact &&
        override.owner->getCanonicalDecl() != canonicalReceiver)
      continue;
    matches.push_back(&override);
  }
  return matches;
}

} // namespace facts::callgraph
